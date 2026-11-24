/*
 * isolation.c — Failure simulation, BFS reachability, and Tarjan bridge detection
 * ValveWise — Teammate B owns this file.
 */

#include "isolation.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════
 * simulate_failure / reset_failures
 * ═══════════════════════════════════════════════════════════════════ */

void simulate_failure(Graph *g, int link_idx) {
    if (!g || link_idx < 0 || link_idx >= g->link_count) return;
    g->links[link_idx].status = STATUS_BURST;
}

void reset_failures(Graph *g) {
    if (!g) return;
    for (int i = 0; i < g->link_count; i++) {
        if (g->links[i].status == STATUS_BURST)
            g->links[i].status = STATUS_OPEN;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * compute_reachability — BFS from all sources
 * ═══════════════════════════════════════════════════════════════════ */

ReachabilityResult *compute_reachability(const Graph *g) {
    if (!g) return NULL;

    ReachabilityResult *r = calloc(1, sizeof(ReachabilityResult));
    if (!r) return NULL;

    r->reachable = calloc((size_t)g->node_count, sizeof(bool));
    if (!r->reachable) { free(r); return NULL; }

    Queue *q = queue_create(g->node_count + 1);
    if (!q) { free(r->reachable); free(r); return NULL; }

    /* Enqueue all source nodes */
    for (int s = 0; s < g->source_count; s++) {
        int src = g->source_indices[s];
        if (!r->reachable[src]) {
            r->reachable[src] = true;
            queue_enqueue(q, src);
        }
    }

    /* BFS */
    int cur;
    while (queue_dequeue(q, &cur)) {
        for (Edge *e = g->adj[cur]; e; e = e->next) {
            Link *lnk = &g->links[e->link_idx];
            /* Skip burst or closed links */
            if (lnk->status == STATUS_BURST || lnk->status == STATUS_CLOSED)
                continue;
            if (!r->reachable[e->to]) {
                r->reachable[e->to] = true;
                queue_enqueue(q, e->to);
            }
        }
    }
    queue_free(q);

    /* Collect disconnected nodes */
    int disc_cap = 16, disc_cnt = 0;
    int *disc = malloc((size_t)disc_cap * sizeof(int));
    if (!disc) { free(r->reachable); free(r); return NULL; }

    double total_demand = 0.0;
    for (int i = 0; i < g->node_count; i++) {
        if (!r->reachable[i]) {
            if (disc_cnt >= disc_cap) {
                int new_cap = disc_cap * 2;
                int *tmp = realloc(disc, (size_t)new_cap * sizeof(int));
                if (tmp) {
                    disc = tmp;
                    disc_cap = new_cap;
                }
            }
            if (disc_cnt < disc_cap) {
                disc[disc_cnt++] = i;
            }
            total_demand += g->nodes[i].base_demand;
        }
    }

    r->disconnected_count   = disc_cnt;
    r->disconnected_demand  = total_demand;
    r->disconnected_nodes   = disc;
    return r;
}

void free_reachability(ReachabilityResult *r) {
    if (!r) return;
    free(r->reachable);
    free(r->disconnected_nodes);
    free(r);
}

/* ═══════════════════════════════════════════════════════════════════
 * compute_isolation — segment closure model
 * ═══════════════════════════════════════════════════════════════════ */

static bool is_valved(const Link *l) {
    return l->link_type == LINK_TYPE_VALVE || l->has_valve;
}

static double network_demand(const Graph *g) {
    double total = 0.0;
    for (int i = 0; i < g->node_count; i++)
        if (!g->nodes[i].is_source) total += g->nodes[i].base_demand;
    return total;
}

/* Build one or more segments. If allow_stuck is true, stuck valves are
 * traversable; otherwise every valved link is a boundary. */
static bool *build_segment(const Graph *g, const bool *failed,
                           const int *failed_links, int failed_count,
                           bool allow_stuck) {
    bool *segment = calloc((size_t)g->node_count, sizeof(bool));
    if (!segment) return NULL;
    Queue *q = queue_create(g->node_count + 1);
    if (!q) { free(segment); return NULL; }
    for (int i = 0; i < failed_count; i++) {
        int li = failed_links[i];
        if (li < 0 || li >= g->link_count || failed[li]) {
            if (li < 0 || li >= g->link_count) continue;
        }
        int ends[2] = {g->links[li].from_node, g->links[li].to_node};
        for (int j = 0; j < 2; j++) {
            if (!segment[ends[j]]) {
                segment[ends[j]] = true;
                queue_enqueue(q, ends[j]);
            }
        }
    }
    int u;
    while (queue_dequeue(q, &u)) {
        for (Edge *e = g->adj[u]; e; e = e->next) {
            const Link *l = &g->links[e->link_idx];
            if (failed[e->link_idx] || l->status == STATUS_BURST) continue;
            if (is_valved(l) && !(allow_stuck && l->status == STATUS_STUCK)) continue;
            if (!segment[e->to]) {
                segment[e->to] = true;
                queue_enqueue(q, e->to);
            }
        }
    }
    queue_free(q);
    return segment;
}

static void apply_statuses(Graph *g, const bool *failed, const bool *closed) {
    for (int i = 0; i < g->link_count; i++) {
        if (failed[i]) g->links[i].status = STATUS_BURST;
        else if (closed[i]) g->links[i].status = STATUS_CLOSED;
    }
}

static ReachabilityResult *measure_plan(Graph *g, const bool *failed, const bool *closed) {
    LinkStatus *saved = malloc((size_t)g->link_count * sizeof(LinkStatus));
    if (!saved) return NULL;
    for (int i = 0; i < g->link_count; i++) saved[i] = g->links[i].status;
    apply_statuses(g, failed, closed);
    ReachabilityResult *r = compute_reachability(g);
    for (int i = 0; i < g->link_count; i++) g->links[i].status = saved[i];
    free(saved);
    return r;
}

IsolationResult *compute_isolation(Graph *g, const int *failed_links, int failed_count) {
    if (!g || !failed_links || failed_count <= 0) return NULL;
    IsolationResult *r = calloc(1, sizeof(IsolationResult));
    bool *failed = calloc((size_t)g->link_count, sizeof(bool));
    bool *segment = NULL, *ideal_segment = NULL, *closed = NULL, *ideal_closed = NULL;
    if (!r || !failed) { free(r); free(failed); return NULL; }
    r->failed_links = malloc((size_t)failed_count * sizeof(int));
    if (!r->failed_links) { free(failed); free(r); return NULL; }
    memcpy(r->failed_links, failed_links, (size_t)failed_count * sizeof(int));
    r->failed_count = failed_count;
    for (int i = 0; i < failed_count; i++)
        if (failed_links[i] >= 0 && failed_links[i] < g->link_count) failed[failed_links[i]] = true;

    segment = build_segment(g, failed, failed_links, failed_count, true);
    ideal_segment = build_segment(g, failed, failed_links, failed_count, false);
    closed = calloc((size_t)g->link_count, sizeof(bool));
    ideal_closed = calloc((size_t)g->link_count, sizeof(bool));
    if (!segment || !ideal_segment || !closed || !ideal_closed) goto fail;

    for (int i = 0; i < g->link_count; i++) {
        Link *l = &g->links[i];
        bool boundary = is_valved(l) && !failed[i] && l->status != STATUS_STUCK;
        bool ideal_boundary = is_valved(l) && !failed[i];
        bool touches = segment[l->from_node] != segment[l->to_node];
        bool ideal_touches = ideal_segment[l->from_node] != ideal_segment[l->to_node];
        if (boundary && (segment[l->from_node] || segment[l->to_node]) && touches) closed[i] = true;
        if (ideal_boundary && (ideal_segment[l->from_node] || ideal_segment[l->to_node]) && ideal_touches)
            ideal_closed[i] = true;
    }
    r->valves_to_close = calloc((size_t)g->link_count, sizeof(int));
    if (!r->valves_to_close) goto fail;
    for (int i = 0; i < g->link_count; i++) if (closed[i]) r->valves_to_close[r->valve_count++] = i;

    r->isolation_feasible = true;
    for (int i = 0; i < g->source_count; i++)
        if (segment[g->source_indices[i]]) { r->isolation_feasible = false; break; }
    r->extra_disrupted = calloc((size_t)g->node_count, sizeof(int));
    if (!r->extra_disrupted) goto fail;
    if (!r->isolation_feasible) {
        r->total_disrupted_demand = network_demand(g);
    } else {
        ReachabilityResult *with_stuck = measure_plan(g, failed, closed);
        ReachabilityResult *without_stuck = measure_plan(g, failed, ideal_closed);
        if (!with_stuck || !without_stuck) { free_reachability(with_stuck); free_reachability(without_stuck); goto fail; }
        r->total_disrupted_demand = with_stuck->disconnected_demand;
        for (int i = 0; i < g->node_count; i++) {
            if (!with_stuck->reachable[i] && without_stuck->reachable[i] && g->nodes[i].base_demand > 0.0)
                r->extra_disrupted[r->extra_disrupted_count++] = i;
        }
        free_reachability(with_stuck);
        free_reachability(without_stuck);
    }
    free(segment); free(ideal_segment); free(closed); free(ideal_closed); free(failed);
    return r;
fail:
    free(segment); free(ideal_segment); free(closed); free(ideal_closed); free(failed);
    free(r->failed_links); free(r->valves_to_close); free(r->extra_disrupted); free(r);
    return NULL;
}

void free_isolation(IsolationResult *r) {
    if (!r) return;
    free(r->failed_links);
    free(r->valves_to_close);
    free(r->extra_disrupted);
    free(r);
}

/* ═══════════════════════════════════════════════════════════════════
 * Tarjan's bridge/articulation-point detection (DFS)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    int *disc;    /* discovery time */
    int *low;     /* low-link value */
    int *parent;  /* parent in DFS tree (-1 for root) */
    int *parent_edge; /* link used to reach each node */
    bool *ap;     /* articulation point flag */
    int   timer;
    /* Output collections */
    int  *bridges;
    int   bridge_cnt, bridge_cap;
    int  *ap_arr;
    int   ap_cnt, ap_cap;
    const Graph *g;
} TarjanCtx;

static void dfs_tarjan(TarjanCtx *ctx, int u) {
    ctx->disc[u] = ctx->low[u] = ctx->timer++;
    int child_count = 0;

    for (Edge *e = ctx->g->adj[u]; e; e = e->next) {
        int v = e->to;
        int lnk_idx = e->link_idx;

        /* Skip only the tree edge itself. A parallel edge to the parent is
         * a real back edge and must prevent the tree edge being a bridge. */
        if (lnk_idx == ctx->parent_edge[u]) continue;

        if (ctx->disc[v] == -1) {
            /* Tree edge */
            child_count++;
            ctx->parent[v] = u;
            ctx->parent_edge[v] = lnk_idx;
            dfs_tarjan(ctx, v);

            /* Update low value */
            if (ctx->low[v] < ctx->low[u]) ctx->low[u] = ctx->low[v];

            /* Bridge check */
            if (ctx->low[v] > ctx->disc[u]) {
                /* lnk_idx is a bridge */
                if (ctx->bridge_cnt >= ctx->bridge_cap) {
                    int new_cap = ctx->bridge_cap * 2;
                    int *tmp = realloc(ctx->bridges, (size_t)new_cap * sizeof(int));
                    if (tmp) {
                        ctx->bridges = tmp;
                        ctx->bridge_cap = new_cap;
                    }
                }
                if (ctx->bridge_cnt < ctx->bridge_cap) {
                    ctx->bridges[ctx->bridge_cnt++] = lnk_idx;
                }
            }

            /* Articulation point check */
            if (ctx->parent[u] == -1 && child_count > 1)
                ctx->ap[u] = true;
            if (ctx->parent[u] != -1 && ctx->low[v] >= ctx->disc[u])
                ctx->ap[u] = true;

        } else {
            /* Back edge */
            if (ctx->disc[v] < ctx->low[u]) ctx->low[u] = ctx->disc[v];
        }
    }
}

StructuralResult *compute_structural_analysis(const Graph *g) {
    if (!g) return NULL;
    int n = g->node_count;

    StructuralResult *sr = calloc(1, sizeof(StructuralResult));
    if (!sr) return NULL;

    sr->component_id = malloc((size_t)n * sizeof(int));
    if (!sr->component_id) { free(sr); return NULL; }
    for (int i = 0; i < n; i++) sr->component_id[i] = -1;

    TarjanCtx ctx = {0};
    ctx.g         = g;
    ctx.disc      = malloc((size_t)n * sizeof(int));
    ctx.low       = malloc((size_t)n * sizeof(int));
    ctx.parent    = malloc((size_t)n * sizeof(int));
    ctx.parent_edge = malloc((size_t)n * sizeof(int));
    ctx.ap        = calloc((size_t)n, sizeof(bool));
    ctx.timer     = 0;
    ctx.bridge_cap = 16; ctx.bridge_cnt = 0;
    ctx.bridges   = malloc(16 * sizeof(int));
    ctx.ap_cap    = 16; ctx.ap_cnt = 0;
    ctx.ap_arr    = malloc(16 * sizeof(int));

    if (!ctx.disc || !ctx.low || !ctx.parent || !ctx.parent_edge || !ctx.ap || !ctx.bridges || !ctx.ap_arr) {
        free(ctx.disc); free(ctx.low); free(ctx.parent); free(ctx.parent_edge);
        free(ctx.ap); free(ctx.bridges); free(ctx.ap_arr);
        free(sr->component_id); free(sr);
        return NULL;
    }

    for (int i = 0; i < n; i++) { ctx.disc[i] = -1; ctx.low[i] = 0; ctx.parent[i] = -1; ctx.parent_edge[i] = -1; }

    for (int i = 0; i < n; i++) {
        if (ctx.disc[i] == -1) {
            dfs_tarjan(&ctx, i);
        }
    }

    /* Label components: assign by connected component via union-find approximation */
    /* Simpler: re-BFS to assign component IDs */
    Queue *q = queue_create(n + 1);
    for (int i = 0; i < n; i++) sr->component_id[i] = -1;
    int comp_id = 0;
    for (int i = 0; i < n; i++) {
        if (sr->component_id[i] == -1) {
            sr->component_id[i] = comp_id;
            queue_enqueue(q, i);
            int cur;
            while (queue_dequeue(q, &cur)) {
                for (Edge *e = g->adj[cur]; e; e = e->next) {
                    if (sr->component_id[e->to] == -1) {
                        sr->component_id[e->to] = comp_id;
                        queue_enqueue(q, e->to);
                    }
                }
            }
            comp_id++;
        }
    }
    queue_free(q);
    sr->component_count = comp_id;

    /* Collect articulation points */
    for (int i = 0; i < n; i++) {
        if (ctx.ap[i]) {
            if (ctx.ap_cnt >= ctx.ap_cap) {
                int new_cap = ctx.ap_cap * 2;
                int *tmp = realloc(ctx.ap_arr, (size_t)new_cap * sizeof(int));
                if (tmp) {
                    ctx.ap_arr = tmp;
                    ctx.ap_cap = new_cap;
                }
            }
            if (ctx.ap_cnt < ctx.ap_cap) {
                ctx.ap_arr[ctx.ap_cnt++] = i;
            }
        }
    }

    sr->bridges             = ctx.bridges;
    sr->bridge_count        = ctx.bridge_cnt;
    sr->articulation_points = ctx.ap_arr;
    sr->ap_count            = ctx.ap_cnt;

    free(ctx.disc); free(ctx.low); free(ctx.parent); free(ctx.parent_edge); free(ctx.ap);
    return sr;
}

void free_structural(StructuralResult *sr) {
    if (!sr) return;
    free(sr->component_id);
    free(sr->bridges);
    free(sr->articulation_points);
    free(sr);
}
