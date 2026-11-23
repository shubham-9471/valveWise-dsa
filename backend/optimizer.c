/*
 * optimizer.c — Valve placement optimization algorithms for ValveWise
 * Teammate B owns this file.
 */

#define _POSIX_C_SOURCE 199309L

#include "optimizer.h"
#include "isolation.h"
#include "heap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

/* ── Wall-clock timer ─────────────────────────────────────────────── */

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* ── Node degree helper ───────────────────────────────────────────── */

static int node_degree(const Graph *g, int node_idx) {
    int deg = 0;
    for (Edge *e = g->adj[node_idx]; e; e = e->next) deg++;
    return deg;
}

/* ── compute_total_disruption ─────────────────────────────────────── */

double compute_total_disruption(Graph *g,
                                const int *valve_links, int valve_count,
                                const int *failure_scenarios, int scenario_count) {
    if (!g) return 0.0;

    /* Temporarily mark these links as having a valve */
    for (int i = 0; i < valve_count; i++)
        if (valve_links[i] >= 0 && valve_links[i] < g->link_count)
            g->links[valve_links[i]].has_valve = true;

    double total = 0.0;
    for (int s = 0; s < scenario_count; s++) {
        int lnk = failure_scenarios[s];
        if (lnk < 0 || lnk >= g->link_count) continue;

        /* Simulate failure */
        LinkStatus prev = g->links[lnk].status;
        g->links[lnk].status = STATUS_BURST;

        /* Compute isolation and resulting disruption */
        IsolationResult *iso = compute_isolation(g, &lnk, 1);
        if (iso) {
            total += iso->total_disrupted_demand;
            free_isolation(iso);
        } else {
            /* Fallback: plain reachability */
            ReachabilityResult *r = compute_reachability(g);
            if (r) { total += r->disconnected_demand; free_reachability(r); }
        }

        g->links[lnk].status = prev;
    }

    /* Restore valve flags */
    for (int i = 0; i < valve_count; i++)
        if (valve_links[i] >= 0 && valve_links[i] < g->link_count)
            g->links[valve_links[i]].has_valve = false;

    return total;
}

/* ── rank_links ───────────────────────────────────────────────────── */

RankedLink *rank_links(const Graph *g,
                       const StructuralResult *sr,
                       RankMode mode,
                       int *out_count) {
    if (!g || !out_count) return NULL;
    int n = g->link_count;
    if (n == 0) { *out_count = 0; return NULL; }

    RankedLink *ranked = malloc((size_t)n * sizeof(RankedLink));
    if (!ranked) return NULL;

    /* Build bridge set for quick lookup */
    bool *is_bridge = calloc((size_t)n, sizeof(bool));
    if (sr) {
        for (int i = 0; i < sr->bridge_count; i++)
            if (sr->bridges[i] >= 0 && sr->bridges[i] < n)
                is_bridge[sr->bridges[i]] = true;
    }

    for (int i = 0; i < n; i++) {
        ranked[i].link_idx = i;
        Link *lnk = &g->links[i];

        switch (mode) {
        case RANK_BY_DISRUPTION: {
            /* Temporarily burst link, measure disruption, restore */
            LinkStatus prev = lnk->status;
            lnk->status = STATUS_BURST;
            ReachabilityResult *r = compute_reachability(g);
            ranked[i].score = r ? r->disconnected_demand : 0.0;
            free_reachability(r);
            lnk->status = prev;
            snprintf(ranked[i].reason, sizeof(ranked[i].reason),
                     "Failure of pipe %s disconnects %.1f L/s of demand",
                     lnk->id, ranked[i].score);
            break;
        }
        case RANK_BY_BETWEENNESS: {
            ranked[i].score = is_bridge[i] ? 1000.0 : (double)(
                node_degree(g, lnk->from_node) + node_degree(g, lnk->to_node));
            snprintf(ranked[i].reason, sizeof(ranked[i].reason),
                     "%s%s (degree sum %d)",
                     is_bridge[i] ? "BRIDGE — " : "",
                     lnk->id,
                     node_degree(g, lnk->from_node) + node_degree(g, lnk->to_node));
            break;
        }
        case RANK_BY_DEGREE:
        default: {
            int da = node_degree(g, lnk->from_node);
            int db = node_degree(g, lnk->to_node);
            ranked[i].score = (double)(da * db);
            snprintf(ranked[i].reason, sizeof(ranked[i].reason),
                     "Pipe %s endpoint degrees: %d × %d = %d",
                     lnk->id, da, db, da * db);
            break;
        }
        }
    }
    free(is_bridge);

    /* Insertion sort descending (n is small for water networks) */
    for (int i = 1; i < n; i++) {
        RankedLink key = ranked[i];
        int j = i - 1;
        while (j >= 0 && ranked[j].score < key.score) {
            ranked[j + 1] = ranked[j];
            j--;
        }
        ranked[j + 1] = key;
    }

    *out_count = n;
    return ranked;
}

/* ── Utility: build full scenario array (all non-valve pipes) ──────── */

static int *build_default_scenarios(const Graph *g, int *out_count) {
    int cap = g->link_count, cnt = 0;
    int *arr = malloc((size_t)cap * sizeof(int));
    if (!arr) { *out_count = 0; return NULL; }
    for (int i = 0; i < g->link_count; i++) {
        if (g->links[i].link_type == LINK_TYPE_PIPE)
            arr[cnt++] = i;
    }
    *out_count = cnt;
    return arr;
}

/* ── optimize_greedy ─────────────────────────────────────────────── */

PlacementResult *optimize_greedy(Graph *g, int budget,
                                  const int *failure_scenarios, int scenario_count) {
    if (!g) return NULL;
    double t0 = now_ms();

    int own_scenarios = 0;
    int *own_sc = NULL;
    if (!failure_scenarios || scenario_count <= 0) {
        own_sc = build_default_scenarios(g, &scenario_count);
        failure_scenarios = own_sc;
        own_scenarios = 1;
    }

    PlacementResult *pr = calloc(1, sizeof(PlacementResult));
    if (!pr) { free(own_sc); return NULL; }
    strncpy(pr->algorithm_name, "Greedy", sizeof(pr->algorithm_name) - 1);

    /* Baseline: no extra valves */
    pr->baseline_disruption = compute_total_disruption(g, NULL, 0,
                                                       failure_scenarios, scenario_count);

    pr->placed_valves = malloc((size_t)(budget + 1) * sizeof(int));
    if (!pr->placed_valves) { free(pr); free(own_sc); return NULL; }
    pr->placed_count = 0;

    bool *chosen = calloc((size_t)g->link_count, sizeof(bool));
    if (!chosen) { free(pr->placed_valves); free(pr); free(own_sc); return NULL; }

    double current_disruption = pr->baseline_disruption;
    long candidates = 0;

    for (int round = 0; round < budget; round++) {
        double best_gain = -1.0;
        int    best_link = -1;

        for (int i = 0; i < g->link_count; i++) {
            if (g->links[i].has_valve || chosen[i]) continue;
            if (g->links[i].link_type == LINK_TYPE_VALVE) continue;

            /* Temporarily add valve at link i */
            g->links[i].has_valve = true;

            /* Evaluate disruption with current chosen set + link i */
            int   trial_set[64]; int trial_cnt = 0;
            for (int j = 0; j < g->link_count && trial_cnt < 64; j++)
                if (chosen[j]) trial_set[trial_cnt++] = j;
            trial_set[trial_cnt++] = i;

            /* Restore flag before measuring (compute_total_disruption sets it internally) */
            g->links[i].has_valve = false;

            double d = compute_total_disruption(g, trial_set, trial_cnt,
                                                failure_scenarios, scenario_count);
            candidates++;
            double gain = current_disruption - d;
            if (gain > best_gain) { best_gain = gain; best_link = i; }
        }

        if (best_link == -1) break; /* No improvement possible */
        chosen[best_link] = true;
        pr->placed_valves[pr->placed_count++] = best_link;
        current_disruption -= best_gain;
    }

    pr->total_disruption    = current_disruption;
    pr->improvement_pct     = pr->baseline_disruption > 0.0
        ? 100.0 * (pr->baseline_disruption - pr->total_disruption) / pr->baseline_disruption
        : 0.0;
    pr->candidates_evaluated = candidates;
    pr->runtime_ms           = now_ms() - t0;

    free(chosen);
    if (own_scenarios) free(own_sc);
    return pr;
}

/* ── optimize_random ──────────────────────────────────────────────── */

PlacementResult *optimize_random(Graph *g, int budget,
                                  const int *failure_scenarios, int scenario_count,
                                  unsigned int seed) {
    if (!g) return NULL;
    double t0 = now_ms();
    srand(seed);

    int own_scenarios = 0;
    int *own_sc = NULL;
    if (!failure_scenarios || scenario_count <= 0) {
        own_sc = build_default_scenarios(g, &scenario_count);
        failure_scenarios = own_sc;
        own_scenarios = 1;
    }

    PlacementResult *pr = calloc(1, sizeof(PlacementResult));
    if (!pr) { free(own_sc); return NULL; }
    strncpy(pr->algorithm_name, "Random", sizeof(pr->algorithm_name) - 1);

    pr->baseline_disruption = compute_total_disruption(g, NULL, 0,
                                                       failure_scenarios, scenario_count);
    pr->placed_valves = malloc((size_t)(budget + 1) * sizeof(int));
    pr->placed_count  = 0;

    /* Collect candidate links (pipes without valves) */
    int *candidates = malloc((size_t)g->link_count * sizeof(int));
    int cand_cnt = 0;
    for (int i = 0; i < g->link_count; i++) {
        if (!g->links[i].has_valve && g->links[i].link_type == LINK_TYPE_PIPE)
            candidates[cand_cnt++] = i;
    }

    /* Fisher-Yates shuffle, pick first budget elements */
    for (int i = 0; i < cand_cnt && pr->placed_count < budget; i++) {
        int j = i + rand() % (cand_cnt - i);
        int tmp = candidates[i]; candidates[i] = candidates[j]; candidates[j] = tmp;
        pr->placed_valves[pr->placed_count++] = candidates[i];
    }
    free(candidates);

    pr->total_disruption = compute_total_disruption(g,
                                                    pr->placed_valves, pr->placed_count,
                                                    failure_scenarios, scenario_count);
    pr->improvement_pct  = pr->baseline_disruption > 0.0
        ? 100.0 * (pr->baseline_disruption - pr->total_disruption) / pr->baseline_disruption
        : 0.0;
    pr->candidates_evaluated = pr->placed_count;
    pr->runtime_ms           = now_ms() - t0;

    if (own_scenarios) free(own_sc);
    return pr;
}

/* ── optimize_topology ────────────────────────────────────────────── */

PlacementResult *optimize_topology(Graph *g, int budget,
                                    const StructuralResult *sr) {
    if (!g) return NULL;
    double t0 = now_ms();

    PlacementResult *pr = calloc(1, sizeof(PlacementResult));
    if (!pr) return NULL;
    strncpy(pr->algorithm_name, "Topology", sizeof(pr->algorithm_name) - 1);

    pr->placed_valves = malloc((size_t)(budget + 1) * sizeof(int));
    pr->placed_count  = 0;

    bool *chosen = calloc((size_t)g->link_count, sizeof(bool));

    /* 1. Add bridges first */
    if (sr) {
        for (int i = 0; i < sr->bridge_count && pr->placed_count < budget; i++) {
            int b = sr->bridges[i];
            if (!g->links[b].has_valve && !chosen[b]) {
                chosen[b] = true;
                pr->placed_valves[pr->placed_count++] = b;
            }
        }
    }

    /* 2. Add high-degree links */
    if (pr->placed_count < budget) {
        /* Use heap to pick top-degree links */
        Heap *h = heap_create(g->link_count + 1, true); /* max-heap */
        for (int i = 0; i < g->link_count; i++) {
            if (!chosen[i] && !g->links[i].has_valve && g->links[i].link_type == LINK_TYPE_PIPE) {
                double score = (double)(node_degree(g, g->links[i].from_node) +
                                        node_degree(g, g->links[i].to_node));
                heap_push(h, score, i);
            }
        }
        HeapNode hn;
        while (pr->placed_count < budget && heap_pop(h, &hn)) {
            if (!chosen[hn.value]) {
                chosen[hn.value] = true;
                pr->placed_valves[pr->placed_count++] = hn.value;
            }
        }
        heap_free(h);
    }

    free(chosen);

    /* Compute disruption */
    int sc_cnt = 0;
    int *sc = build_default_scenarios(g, &sc_cnt);
    pr->baseline_disruption = compute_total_disruption(g, NULL, 0, sc, sc_cnt);
    pr->total_disruption    = compute_total_disruption(g,
                                                       pr->placed_valves, pr->placed_count,
                                                       sc, sc_cnt);
    free(sc);
    pr->improvement_pct  = pr->baseline_disruption > 0.0
        ? 100.0 * (pr->baseline_disruption - pr->total_disruption) / pr->baseline_disruption
        : 0.0;
    pr->candidates_evaluated = pr->placed_count;
    pr->runtime_ms           = now_ms() - t0;
    return pr;
}

/* ── optimize_exhaustive ─────────────────────────────────────────── */

/* Combinatorial helper: n choose k */
static long long n_choose_k(int n, int k) {
    if (k > n) return 0;
    if (k == 0 || k == n) return 1;
    if (k > n - k) k = n - k;
    long long result = 1;
    for (int i = 0; i < k; i++) {
        result = result * (n - i) / (i + 1);
        if (result > 500001) return result; /* early exit */
    }
    return result;
}

static double best_exhaustive;
static int   *best_set_ex;
static int    best_set_cnt;

static void enumerate(Graph *g, int *candidates, int cand_cnt,
                       int start, int k, int *current, int depth,
                       const int *failure_scenarios, int scenario_count) {
    if (depth == k) {
        double d = compute_total_disruption(g, current, k,
                                            failure_scenarios, scenario_count);
        if (d < best_exhaustive) {
            best_exhaustive = d;
            memcpy(best_set_ex, current, (size_t)k * sizeof(int));
            best_set_cnt = k;
        }
        return;
    }
    for (int i = start; i < cand_cnt; i++) {
        current[depth] = candidates[i];
        enumerate(g, candidates, cand_cnt, i + 1, k, current, depth + 1,
                  failure_scenarios, scenario_count);
    }
}

PlacementResult *optimize_exhaustive(Graph *g, int budget,
                                      const int *failure_scenarios, int scenario_count) {
    if (!g) return NULL;
    double t0 = now_ms();

    int *candidates = malloc((size_t)g->link_count * sizeof(int));
    int cand_cnt = 0;
    for (int i = 0; i < g->link_count; i++) {
        if (!g->links[i].has_valve && g->links[i].link_type == LINK_TYPE_PIPE)
            candidates[cand_cnt++] = i;
    }

    long long combos = n_choose_k(cand_cnt, budget);
    if (combos > 500000) {
        free(candidates);
        return NULL; /* Too large */
    }

    int own_scenarios = 0;
    int *own_sc = NULL;
    if (!failure_scenarios || scenario_count <= 0) {
        own_sc = build_default_scenarios(g, &scenario_count);
        failure_scenarios = own_sc;
        own_scenarios = 1;
    }

    PlacementResult *pr = calloc(1, sizeof(PlacementResult));
    pr->placed_valves = malloc((size_t)(budget + 1) * sizeof(int));
    pr->placed_count  = 0;
    strncpy(pr->algorithm_name, "Exhaustive", sizeof(pr->algorithm_name) - 1);
    pr->baseline_disruption = compute_total_disruption(g, NULL, 0,
                                                       failure_scenarios, scenario_count);

    best_exhaustive  = pr->baseline_disruption + 1.0;
    best_set_ex      = malloc((size_t)(budget + 1) * sizeof(int));
    best_set_cnt     = 0;

    int *current = malloc((size_t)(budget + 1) * sizeof(int));
    enumerate(g, candidates, cand_cnt, 0, budget, current, 0,
              failure_scenarios, scenario_count);
    free(current);

    memcpy(pr->placed_valves, best_set_ex, (size_t)best_set_cnt * sizeof(int));
    pr->placed_count      = best_set_cnt;
    pr->total_disruption  = best_exhaustive;
    pr->improvement_pct   = pr->baseline_disruption > 0.0
        ? 100.0 * (pr->baseline_disruption - pr->total_disruption) / pr->baseline_disruption
        : 0.0;
    pr->candidates_evaluated = (long)combos;
    pr->runtime_ms           = now_ms() - t0;

    free(best_set_ex);
    free(candidates);
    if (own_scenarios) free(own_sc);
    return pr;
}

/* ── run_full_optimization ───────────────────────────────────────── */

OptimizationReport *run_full_optimization(Graph *g, int budget,
                                           const int *failure_scenarios, int scenario_count,
                                           const StructuralResult *sr,
                                           unsigned int seed) {
    OptimizationReport *report = calloc(1, sizeof(OptimizationReport));
    if (!report) return NULL;
    report->budget     = budget;
    report->greedy     = optimize_greedy(g, budget, failure_scenarios, scenario_count);
    report->random_pl  = optimize_random(g, budget, failure_scenarios, scenario_count, seed);
    report->topology   = optimize_topology(g, budget, sr);
    report->exhaustive = optimize_exhaustive(g, budget, failure_scenarios, scenario_count);
    return report;
}

/* ── free helpers ─────────────────────────────────────────────────── */

void free_placement_result(PlacementResult *pr) {
    if (!pr) return;
    free(pr->placed_valves);
    free(pr);
}

void free_optimization_report(OptimizationReport *report) {
    if (!report) return;
    free_placement_result(report->greedy);
    free_placement_result(report->random_pl);
    free_placement_result(report->topology);
    free_placement_result(report->exhaustive);
    free(report);
}
