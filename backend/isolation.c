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
 * compute_isolation — isolation boundary with stuck-valve handling
 * ═══════════════════════════════════════════════════════════════════ */

IsolationResult *compute_isolation(Graph *g,
                                   const int *failed_links,
                                   int        failed_count) {
    if (!g || !failed_links || failed_count <= 0) return NULL;

    IsolationResult *r = calloc(1, sizeof(IsolationResult));
    if (!r) return NULL;

    /* Copy failed link list */
    r->failed_links = malloc((size_t)failed_count * sizeof(int));
    if (!r->failed_links) { free(r); return NULL; }
    memcpy(r->failed_links, failed_links, (size_t)failed_count * sizeof(int));
    r->failed_count = failed_count;

    /* Track which link indices have been added as valves to avoid duplicates */
    bool *valve_added = calloc((size_t)g->link_count, sizeof(bool));
    bool *node_visited = calloc((size_t)g->node_count, sizeof(bool));
    if (!valve_added || !node_visited) {
        free(valve_added); free(node_visited);
        free(r->failed_links); free(r);
        return NULL;
    }

    int vtc_cap = 16, vtc_cnt = 0;
    int *valves_to_close = malloc((size_t)vtc_cap * sizeof(int));

    int ed_cap = 16, ed_cnt = 0;
    int *extra_disrupted = malloc((size_t)ed_cap * sizeof(int));

    if (!valves_to_close || !extra_disrupted) {
        free(valve_added); free(node_visited);
        free(valves_to_close); free(extra_disrupted);
        free(r->failed_links); free(r);
        return NULL;
    }

    /* For each failed link, BFS from both endpoints to find bounding valves */
    for (int fi = 0; fi < failed_count; fi++) {
        int lnk_idx = failed_links[fi];
        if (lnk_idx < 0 || lnk_idx >= g->link_count) continue;

        int endpoints[2] = { g->links[lnk_idx].from_node, g->links[lnk_idx].to_node };

        for (int ep = 0; ep < 2; ep++) {
            /* BFS from this endpoint, stopping at valves */
            Queue *q = queue_create(g->node_count + 1);
            if (!q) continue;

            memset(node_visited, 0, (size_t)g->node_count * sizeof(bool));
            int start = endpoints[ep];
            node_visited[start] = true;
            queue_enqueue(q, start);

            int cur;
            while (queue_dequeue(q, &cur)) {
                for (Edge *e = g->adj[cur]; e; e = e->next) {
                    Link *lnk = &g->links[e->link_idx];

                    /* Don't traverse back through failed link */
                    if (e->link_idx == lnk_idx) continue;
                    if (lnk->status == STATUS_BURST) continue;

                    if (lnk->has_valve || lnk->link_type == LINK_TYPE_VALVE) {
                        /* Found a bounding valve */
                        if (!valve_added[e->link_idx]) {
                            valve_added[e->link_idx] = true;
                            if (lnk->status == STATUS_STUCK) {
                                /* Stuck-open: cannot close; record extra disruption on the far node */
                                if (ed_cnt >= ed_cap) {
                                    int new_cap = ed_cap * 2;
                                    int *tmp = realloc(extra_disrupted, (size_t)new_cap * sizeof(int));
                                    if (tmp) {
                                        extra_disrupted = tmp;
                                        ed_cap = new_cap;
                                    }
                                }
                                if (ed_cnt < ed_cap) {
                                    extra_disrupted[ed_cnt++] = e->to;
                                }
                                /* Continue BFS past stuck valve */
                                if (!node_visited[e->to]) {
                                    node_visited[e->to] = true;
                                    queue_enqueue(q, e->to);
                                }
                            } else {
                                /* Operable valve — add to close list */
                                if (vtc_cnt >= vtc_cap) {
                                    int new_cap = vtc_cap * 2;
                                    int *tmp = realloc(valves_to_close, (size_t)new_cap * sizeof(int));
                                    if (tmp) {
                                        valves_to_close = tmp;
                                        vtc_cap = new_cap;
                                    }
                                }
                                if (vtc_cnt < vtc_cap) {
                                    valves_to_close[vtc_cnt++] = e->link_idx;
                                }
                            }
                        }
                        /* Do NOT continue BFS through a closeable valve */
                        if (lnk->status != STATUS_STUCK) continue;
                    }

                    /* Traverse open pipes */
                    if (!node_visited[e->to]) {
                        node_visited[e->to] = true;
                        queue_enqueue(q, e->to);
                    }
                }
            }
            queue_free(q);
        }
    }

    free(valve_added);
    free(node_visited);

    r->valves_to_close        = valves_to_close;
    r->valve_count            = vtc_cnt;
    r->extra_disrupted        = extra_disrupted;
    r->extra_disrupted_count  = ed_cnt;

    /* Save previous link statuses before applying isolation */
    LinkStatus *prev_valve_status = malloc((size_t)vtc_cnt * sizeof(LinkStatus));
    LinkStatus *prev_failed_status = malloc((size_t)failed_count * sizeof(LinkStatus));
    if (!prev_valve_status || !prev_failed_status) {
        free(prev_valve_status);
        free(prev_failed_status);
        free(valves_to_close);
        free(extra_disrupted);
        free(r->failed_links);
        free(r);
        return NULL;
    }

    for (int i = 0; i < vtc_cnt; i++) {
        prev_valve_status[i] = g->links[valves_to_close[i]].status;
        g->links[valves_to_close[i]].status = STATUS_CLOSED;
    }
    for (int fi = 0; fi < failed_count; fi++) {
        prev_failed_status[fi] = g->links[failed_links[fi]].status;
        g->links[failed_links[fi]].status = STATUS_BURST;
    }

    ReachabilityResult *reach = compute_reachability(g);
    r->total_disrupted_demand = reach ? reach->disconnected_demand : 0.0;
    free_reachability(reach);

    /* Restore original statuses accurately */
    for (int i = 0; i < vtc_cnt; i++)
        g->links[valves_to_close[i]].status = prev_valve_status[i];
    for (int fi = 0; fi < failed_count; fi++)
        g->links[failed_links[fi]].status = prev_failed_status[fi];

    free(prev_valve_status);
    free(prev_failed_status);

    return r;
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

        if (ctx->disc[v] == -1) {
            /* Tree edge */
            child_count++;
            ctx->parent[v] = u;
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

        } else if (v != ctx->parent[u]) {
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
    ctx.ap        = calloc((size_t)n, sizeof(bool));
    ctx.timer     = 0;
    ctx.bridge_cap = 16; ctx.bridge_cnt = 0;
    ctx.bridges   = malloc(16 * sizeof(int));
    ctx.ap_cap    = 16; ctx.ap_cnt = 0;
    ctx.ap_arr    = malloc(16 * sizeof(int));

    if (!ctx.disc || !ctx.low || !ctx.parent || !ctx.ap || !ctx.bridges || !ctx.ap_arr) {
        free(ctx.disc); free(ctx.low); free(ctx.parent);
        free(ctx.ap); free(ctx.bridges); free(ctx.ap_arr);
        free(sr->component_id); free(sr);
        return NULL;
    }

    for (int i = 0; i < n; i++) { ctx.disc[i] = -1; ctx.low[i] = 0; ctx.parent[i] = -1; }

    int comp = 0;
    for (int i = 0; i < n; i++) {
        if (ctx.disc[i] == -1) {
            dfs_tarjan(&ctx, i);
            /* Label this component */
            /* (simple labelling: re-walk the disc array) */
            for (int j = 0; j < n; j++) {
                if (sr->component_id[j] == -1 && ctx.disc[j] != -1) {
                    /* Every node reached in this DFS iteration belongs to component comp */
                    /* We use a second pass: mark all nodes discovered after the previous comp */
                }
            }
            comp++;
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

    free(ctx.disc); free(ctx.low); free(ctx.parent); free(ctx.ap);
    return sr;
}

void free_structural(StructuralResult *sr) {
    if (!sr) return;
    free(sr->component_id);
    free(sr->bridges);
    free(sr->articulation_points);
    free(sr);
}
