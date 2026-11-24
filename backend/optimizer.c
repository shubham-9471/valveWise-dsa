/* optimizer.c — comparable valve placement algorithms for ValveWise */
#define _POSIX_C_SOURCE 199309L
#include "optimizer.h"
#include "isolation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#ifndef _WIN32
#include <sys/resource.h>
#endif

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static int node_degree(const Graph *g, int node_idx) {
    int degree = 0;
    for (Edge *e = g->adj[node_idx]; e; e = e->next) degree++;
    return degree;
}

static bool candidate_allowed(const Link *l) {
    return !l->has_valve && l->link_type != LINK_TYPE_VALVE && l->link_type != LINK_TYPE_PUMP;
}

static int *candidate_links(const Graph *g, int *count) {
    int *out = malloc((size_t)(g->link_count > 0 ? g->link_count : 1) * sizeof(int));
    if (!out) { *count = 0; return NULL; }
    *count = 0;
    for (int i = 0; i < g->link_count; i++)
        if (candidate_allowed(&g->links[i])) out[(*count)++] = i;
    return out;
}

ScenarioSet *scenario_set_all(const Graph *g) {
    if (!g) return NULL;
    ScenarioSet *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->failures = calloc((size_t)(g->link_count > 0 ? g->link_count : 1), sizeof(int *));
    s->failure_counts = calloc((size_t)(g->link_count > 0 ? g->link_count : 1), sizeof(int));
    if (!s->failures || !s->failure_counts) { free_scenario_set(s); return NULL; }
    for (int i = 0; i < g->link_count; i++) {
        if (g->links[i].link_type == LINK_TYPE_PUMP) continue;
        s->failures[s->count] = malloc(sizeof(int));
        if (!s->failures[s->count]) { free_scenario_set(s); return NULL; }
        s->failures[s->count][0] = i;
        s->failure_counts[s->count++] = 1;
    }
    return s;
}

ScenarioSet *scenario_set_failed(const int *failed_links, int failed_count) {
    if (failed_count < 0) return NULL;
    ScenarioSet *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->failures = calloc(1, sizeof(int *));
    s->failure_counts = calloc(1, sizeof(int));
    if (!s->failures || !s->failure_counts) { free_scenario_set(s); return NULL; }
    if (failed_count > 0) {
        s->failures[0] = malloc((size_t)failed_count * sizeof(int));
        if (!s->failures[0]) { free_scenario_set(s); return NULL; }
        memcpy(s->failures[0], failed_links, (size_t)failed_count * sizeof(int));
        s->failure_counts[0] = failed_count;
        s->count = 1;
    }
    return s;
}

void free_scenario_set(ScenarioSet *s) {
    if (!s) return;
    for (int i = 0; i < s->count; i++) free(s->failures[i]);
    free(s->failures); free(s->failure_counts); free(s);
}

typedef struct {
    Graph *g;
    const ScenarioSet *scenarios;
    int *candidates;
    int candidate_count;
    double baseline;
    const StructuralResult *structural;
} OptCtx;

/* The one evaluator used by baseline and every placement algorithm. */
static double evaluate(const OptCtx *ctx, const int *valves, int valve_count) {
    Graph *g = ctx->g;
    bool *changed = calloc((size_t)g->link_count, sizeof(bool));
    if (!changed) return 0.0;
    for (int i = 0; i < valve_count; i++) {
        int li = valves[i];
        if (li >= 0 && li < g->link_count && !g->links[li].has_valve) {
            g->links[li].has_valve = true;
            changed[li] = true;
        }
    }
    double total = 0.0;
    for (int s = 0; s < ctx->scenarios->count; s++) {
        IsolationResult *iso = compute_isolation(g, ctx->scenarios->failures[s],
                                                  ctx->scenarios->failure_counts[s]);
        if (iso) { total += iso->total_disrupted_demand; free_isolation(iso); }
    }
    for (int i = 0; i < g->link_count; i++) if (changed[i]) g->links[i].has_valve = false;
    return total;
}

double compute_total_disruption(Graph *g, const int *valve_links, int valve_count,
                                const int *failure_scenarios, int scenario_count) {
    if (!g || scenario_count <= 0) return 0.0;
    ScenarioSet set = {0};
    set.count = scenario_count;
    set.failures = calloc((size_t)scenario_count, sizeof(int *));
    set.failure_counts = calloc((size_t)scenario_count, sizeof(int));
    if (!set.failures || !set.failure_counts) { free(set.failures); free(set.failure_counts); return 0.0; }
    for (int i = 0; i < scenario_count; i++) {
        set.failures[i] = (int *)&failure_scenarios[i];
        set.failure_counts[i] = 1;
    }
    int count = 0; int *candidates = candidate_links(g, &count);
    OptCtx ctx = {g, &set, candidates, count, 0.0, NULL};
    double result = evaluate(&ctx, valve_links, valve_count);
    free(candidates); free(set.failures); free(set.failure_counts);
    return result;
}

/* Structural ranking remains part of the v1 backend API. */
RankedLink *rank_links(const Graph *g, const StructuralResult *sr, RankMode mode, int *out_count) {
    if (!g || !out_count) return NULL;
    int n = g->link_count;
    RankedLink *ranked = malloc((size_t)(n > 0 ? n : 1) * sizeof(*ranked));
    bool *is_bridge = calloc((size_t)(n > 0 ? n : 1), sizeof(bool));
    if (!ranked || !is_bridge) { free(ranked); free(is_bridge); return NULL; }
    if (sr) for (int i = 0; i < sr->bridge_count; i++)
        if (sr->bridges[i] >= 0 && sr->bridges[i] < n) is_bridge[sr->bridges[i]] = true;
    for (int i = 0; i < n; i++) {
        Link *l = &g->links[i]; ranked[i].link_idx = i;
        int da = node_degree(g, l->from_node), db = node_degree(g, l->to_node);
        if (mode == RANK_BY_BETWEENNESS) ranked[i].score = is_bridge[i] ? 1000.0 : da + db;
        else if (mode == RANK_BY_DEGREE) ranked[i].score = (double)da * db;
        else {
            /* In an undirected connectivity graph, removing a non-bridge
             * cannot disconnect any node.  Avoid a full BFS for every link;
             * bridge links still receive their exact demand score. */
            ranked[i].score = 0.0;
            if (is_bridge[i] || !sr) {
                LinkStatus old = l->status; l->status = STATUS_BURST;
                ReachabilityResult *rr = compute_reachability(g);
                ranked[i].score = rr ? rr->disconnected_demand : 0.0;
                free_reachability(rr); l->status = old;
            }
        }
        snprintf(ranked[i].reason, sizeof(ranked[i].reason), "Link %s score %.1f", l->id, ranked[i].score);
    }
    free(is_bridge);
    for (int i = 1; i < n; i++) {
        RankedLink key = ranked[i]; int j = i - 1;
        while (j >= 0 && (ranked[j].score < key.score ||
               (ranked[j].score == key.score && ranked[j].link_idx > key.link_idx))) {
            ranked[j + 1] = ranked[j];
            j--;
        }
        ranked[j + 1] = key;
    }
    *out_count = n; return ranked;
}

static PlacementResult *result_new(const char *name, const OptCtx *ctx, int budget) {
    PlacementResult *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    strncpy(r->algorithm_name, name, sizeof(r->algorithm_name) - 1);
    r->baseline_disruption = ctx->baseline;
    r->best_disruption = ctx->baseline;
    r->mean_disruption = ctx->baseline;
    int cap = budget < ctx->candidate_count ? budget : ctx->candidate_count;
    if (cap < 0) cap = 0;
    r->placed_valves = calloc((size_t)(cap > 0 ? cap : 1), sizeof(int));
    r->placement_before = calloc((size_t)(cap > 0 ? cap : 1), sizeof(double));
    r->placement_after = calloc((size_t)(cap > 0 ? cap : 1), sizeof(double));
    if (!r->placed_valves || !r->placement_before || !r->placement_after) {
        free(r->placed_valves); free(r->placement_before); free(r->placement_after); free(r); return NULL;
    }
    return r;
}

static void finish_result(PlacementResult *r, double total, double t0) {
    r->total_disruption = total;
    r->best_disruption = total;
    r->improvement_pct = r->baseline_disruption > 0.0
        ? 100.0 * (r->baseline_disruption - total) / r->baseline_disruption : 0.0;
    r->runtime_ms = now_ms() - t0;
}

static PlacementResult *optimize_greedy(const OptCtx *ctx, int budget) {
    double t0 = now_ms(); PlacementResult *r = result_new("Greedy", ctx, budget);
    if (!r) return NULL;
    bool *chosen = calloc((size_t)ctx->g->link_count, sizeof(bool));
    int *trial = malloc((size_t)(ctx->candidate_count + 1) * sizeof(int));
    double current = ctx->baseline;
    for (int round = 0; round < budget && round < ctx->candidate_count; round++) {
        double best_gain = -INFINITY, best_value = current; int best = -1;
        for (int i = 0; i < ctx->candidate_count; i++) {
            int li = ctx->candidates[i]; if (chosen[li]) continue;
            int n = 0; for (int j = 0; j < ctx->candidate_count; j++) if (chosen[ctx->candidates[j]]) trial[n++] = ctx->candidates[j];
            trial[n++] = li;
            double d = evaluate(ctx, trial, n); r->candidates_evaluated++;
            double gain = current - d;
            if (gain > best_gain + 1e-12) { best_gain = gain; best = li; best_value = d; }
        }
        if (best < 0 || best_gain <= 1e-9) break;
        chosen[best] = true;
        r->placement_before[r->placed_count] = current;
        r->placement_after[r->placed_count] = best_value;
        r->placed_valves[r->placed_count++] = best; current = best_value;
    }
    free(trial); free(chosen); finish_result(r, current, t0); return r;
}

static unsigned next_random(unsigned *state) {
    *state ^= *state << 13; *state ^= *state >> 17; *state ^= *state << 5; return *state;
}

static PlacementResult *optimize_random(const OptCtx *ctx, int budget, unsigned seed) {
    double t0 = now_ms(); PlacementResult *r = result_new("Random", ctx, budget);
    if (!r) return NULL;
    r->seed = seed;
    const int trials = 100; r->trials = trials;
    int k = budget < ctx->candidate_count ? budget : ctx->candidate_count;
    int *order = malloc((size_t)(ctx->candidate_count > 0 ? ctx->candidate_count : 1) * sizeof(int));
    int *best = malloc((size_t)(k > 0 ? k : 1) * sizeof(int));
    if (!order || !best) { free(order); free(best); free_placement_result(r); return NULL; }
    unsigned state = seed ? seed : 1u; double sum = 0.0, best_d = INFINITY;
    for (int trial_no = 0; trial_no < trials; trial_no++) {
        for (int i = 0; i < ctx->candidate_count; i++) order[i] = ctx->candidates[i];
        for (int i = 0; i < k; i++) { int j = i + (int)(next_random(&state) % (unsigned)(ctx->candidate_count - i)); int t = order[i]; order[i] = order[j]; order[j] = t; }
        double d = evaluate(ctx, order, k); sum += d; r->candidates_evaluated++;
        if (d < best_d) { best_d = d; memcpy(best, order, (size_t)k * sizeof(int)); }
    }
    if (trials == 0) best_d = ctx->baseline;
    memcpy(r->placed_valves, best, (size_t)k * sizeof(int)); r->placed_count = k;
    r->mean_disruption = sum / trials; finish_result(r, best_d, t0);
    for (int i = 0; i < k; i++) {
        r->placement_before[i] = evaluate(ctx, r->placed_valves, i);
        r->placement_after[i] = evaluate(ctx, r->placed_valves, i + 1);
    }
    free(order); free(best); return r;
}

static const Graph *sort_graph;
static bool *sort_bridges;
static int compare_topology(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    int dx = node_degree(sort_graph, sort_graph->links[x].from_node) * node_degree(sort_graph, sort_graph->links[x].to_node);
    int dy = node_degree(sort_graph, sort_graph->links[y].from_node) * node_degree(sort_graph, sort_graph->links[y].to_node);
    if (sort_bridges[x] != sort_bridges[y]) return sort_bridges[x] ? -1 : 1;
    if (dx != dy) return dx > dy ? -1 : 1;
    return x - y;
}

static PlacementResult *optimize_topology(const OptCtx *ctx, int budget) {
    double t0 = now_ms(); PlacementResult *r = result_new("Topology", ctx, budget);
    if (!r) return NULL;
    bool *bridges = calloc((size_t)(ctx->g->link_count > 0 ? ctx->g->link_count : 1), sizeof(bool));
    int *order = malloc((size_t)(ctx->candidate_count > 0 ? ctx->candidate_count : 1) * sizeof(int));
    if (!bridges || !order) { free(bridges); free(order); free_placement_result(r); return NULL; }
    if (ctx->structural) for (int i = 0; i < ctx->structural->bridge_count; i++) bridges[ctx->structural->bridges[i]] = true;
    memcpy(order, ctx->candidates, (size_t)ctx->candidate_count * sizeof(int)); sort_graph = ctx->g; sort_bridges = bridges;
    qsort(order, (size_t)ctx->candidate_count, sizeof(int), compare_topology);
    int k = budget < ctx->candidate_count ? budget : ctx->candidate_count;
    memcpy(r->placed_valves, order, (size_t)k * sizeof(int)); r->placed_count = k;
    double d = evaluate(ctx, r->placed_valves, k); r->candidates_evaluated = 1;
    for (int i = 0; i < k; i++) {
        r->placement_before[i] = evaluate(ctx, r->placed_valves, i);
        r->placement_after[i] = evaluate(ctx, r->placed_valves, i + 1);
    }
    finish_result(r, d, t0); free(bridges); free(order); return r;
}

static long long combination_count(int n, int k, long long limit) {
    if (k < 0 || k > n) return 0;
    if (k > n - k) k = n - k;
    long long result = 1;
    for (int i = 1; i <= k; i++) {
        if (result > limit / (n - k + i)) return limit + 1;
        result = result * (n - k + i) / i;
        if (result > limit) return limit + 1;
    }
    return result;
}

typedef struct { const OptCtx *ctx; int *current; int *best; int k; double value; long long count; } EnumCtx;
static void enumerate(EnumCtx *e, int start, int depth) {
    if (depth == e->k) {
        e->count++; double d = evaluate(e->ctx, e->current, e->k);
        if (d < e->value) { e->value = d; memcpy(e->best, e->current, (size_t)e->k * sizeof(int)); }
        return;
    }
    for (int i = start; i <= e->ctx->candidate_count - (e->k - depth); i++) {
        e->current[depth] = e->ctx->candidates[i]; enumerate(e, i + 1, depth + 1);
    }
}

static PlacementResult *optimize_exhaustive(const OptCtx *ctx, int budget) {
    double t0 = now_ms(); PlacementResult *r = result_new("Exhaustive", ctx, budget);
    if (!r) return NULL;
    int k = budget < ctx->candidate_count ? budget : ctx->candidate_count;
    long long combos = combination_count(ctx->candidate_count, k, 500000);
    if (combos > 500000) {
        r->skipped = true; strcpy(r->skip_reason, "combination count exceeds 500000 cap");
        r->trials = 0; finish_result(r, ctx->baseline, t0); return r;
    }
    int *current = malloc((size_t)(k > 0 ? k : 1) * sizeof(int));
    int *best = malloc((size_t)(k > 0 ? k : 1) * sizeof(int));
    if (!current || !best) { free(current); free(best); free_placement_result(r); return NULL; }
    EnumCtx e = {ctx, current, best, k, INFINITY, 0}; enumerate(&e, 0, 0);
    memcpy(r->placed_valves, best, (size_t)k * sizeof(int)); r->placed_count = k;
    r->candidates_evaluated = e.count; finish_result(r, e.value, t0);
    for (int i = 0; i < k; i++) {
        r->placement_before[i] = evaluate(ctx, r->placed_valves, i);
        r->placement_after[i] = evaluate(ctx, r->placed_valves, i + 1);
    }
    free(current); free(best); return r;
}

OptimizationReport *run_full_optimization(Graph *g, int budget, const ScenarioSet *scenarios,
                                           const StructuralResult *sr, unsigned int seed) {
    if (!g || !scenarios) return NULL;
    double t0 = now_ms();
    OptimizationReport *report = calloc(1, sizeof(*report)); if (!report) return NULL;
    int candidate_count = 0; int *candidates = candidate_links(g, &candidate_count);
    OptCtx ctx = {g, scenarios, candidates, candidate_count, 0.0, sr};
    ctx.baseline = evaluate(&ctx, NULL, 0);
    report->budget = budget; report->seed = seed; report->scenario_count = scenarios->count; report->scenarios = scenarios;
    report->greedy = optimize_greedy(&ctx, budget);
    report->random_pl = optimize_random(&ctx, budget, seed);
    report->topology = optimize_topology(&ctx, budget);
    report->exhaustive = optimize_exhaustive(&ctx, budget);
    report->optimization_ms = now_ms() - t0;
#ifdef _WIN32
    report->memory_kb = -1;
#else
    struct rusage usage; report->memory_kb = getrusage(RUSAGE_SELF, &usage) == 0 ? usage.ru_maxrss : -1;
#endif
    free(candidates); return report;
}

void free_placement_result(PlacementResult *r) { if (r) { free(r->placed_valves); free(r->placement_before); free(r->placement_after); free(r); } }
void free_optimization_report(OptimizationReport *r) {
    if (!r) return;
    free_placement_result(r->greedy); free_placement_result(r->random_pl);
    free_placement_result(r->topology); free_placement_result(r->exhaustive); free(r);
}
