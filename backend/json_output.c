/*
 * json_output.c — JSON serialization implementation for ValveWise
 * Teammate B owns this file.
 *
 * Uses a simple growable string-buffer (SB) approach — no external libraries.
 */

#include "json_output.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

/* ═══════════════════════════════════════════════════════════════════
 * Growable string buffer
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
    bool oom;
} SB;

static void sb_init(SB *sb) {
    sb->buf = calloc(4096, 1);
    sb->cap = 4096;
    sb->len = 0;
    sb->oom = (sb->buf == NULL);
}

static void sb_grow(SB *sb, size_t needed) {
    if (sb->oom || needed > SIZE_MAX - sb->len) { sb->oom = true; return; }
    while (sb->len + needed >= sb->cap) {
        if (sb->cap > SIZE_MAX / 2) { sb->oom = true; return; }
        sb->cap *= 2;
        char *tmp = realloc(sb->buf, sb->cap);
        if (!tmp) { sb->oom = true; return; }
        sb->buf = tmp;
    }
}

static void sb_append(SB *sb, const char *s) {
    if (sb->oom || !s) return;
    size_t slen = strlen(s);
    if (slen == SIZE_MAX) { sb->oom = true; return; }
    sb_grow(sb, slen + 1);
    if (sb->oom) return;
    memcpy(sb->buf + sb->len, s, slen);
    sb->len += slen;
    sb->buf[sb->len] = '\0';
}

static void sb_printf(SB *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (needed < 0) {
        sb->oom = true;
        va_end(ap);
        return;
    }

    char *tmp = malloc((size_t)needed + 1);
    if (!tmp) {
        sb->oom = true;
        va_end(ap);
        return;
    }

    vsnprintf(tmp, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    sb_append(sb, tmp);
    free(tmp);
}

static void sb_append_escaped(SB *sb, const char *s) {
    if (!s) return;
    for (const char *p = s; *p; p++) {
        if (*p == '"') {
            sb_append(sb, "\\\"");
        } else if (*p == '\\') {
            sb_append(sb, "\\\\");
        } else if (*p == '\b') {
            sb_append(sb, "\\b");
        } else if (*p == '\f') {
            sb_append(sb, "\\f");
        } else if (*p == '\n') {
            sb_append(sb, "\\n");
        } else if (*p == '\r') {
            sb_append(sb, "\\r");
        } else if (*p == '\t') {
            sb_append(sb, "\\t");
        } else {
            char ch[2] = { *p, '\0' };
            sb_append(sb, ch);
        }
    }
}

static char *sb_take(SB *sb) {
    /* Caller owns the returned string; free with json_free() */
    if (sb->oom) { free(sb->buf); sb->buf = NULL; return NULL; }
    if (sb->len == SIZE_MAX) { free(sb->buf); sb->buf = NULL; return NULL; }
    char *result = realloc(sb->buf, sb->len + 1);
    if (!result) result = sb->buf;
    result[sb->len] = '\0';
    sb->buf = NULL; sb->len = 0; sb->cap = 0;
    return result;
}

/* ── Helpers for JSON escaping ───────────────────────────────────── */

static const char *node_type_str(NodeType t) {
    switch (t) {
        case NODE_RESERVOIR: return "RESERVOIR";
        case NODE_TANK:      return "TANK";
        default:             return "JUNCTION";
    }
}

static const char *link_status_str(LinkStatus s) {
    switch (s) {
        case STATUS_CLOSED: return "CLOSED";
        case STATUS_BURST:  return "BURST";
        case STATUS_STUCK:  return "STUCK";
        default:            return "OPEN";
    }
}

static const char *link_type_str(int t) {
    return (t == LINK_TYPE_VALVE) ? "VALVE" : "PIPE";
}

/* ── json_graph ──────────────────────────────────────────────────── */

char *json_graph(const Graph *g) {
    if (!g) return NULL;
    SB sb; sb_init(&sb);
    sb_append(&sb, "{\"nodes\":[");
    for (int i = 0; i < g->node_count; i++) {
        const Node *n = &g->nodes[i];
        if (i) sb_append(&sb, ",");
        sb_append(&sb, "{\"id\":\"");
        sb_append_escaped(&sb, n->id);
        sb_printf(&sb,
            "\",\"type\":\"%s\",\"elevation\":%.3f,"
            "\"base_demand\":%.3f,\"x\":%.3f,\"y\":%.3f,\"is_source\":%s}",
            node_type_str(n->type), n->elevation,
            n->base_demand, n->x, n->y,
            n->is_source ? "true" : "false");
    }
    sb_append(&sb, "],\"links\":[");
    for (int i = 0; i < g->link_count; i++) {
        const Link *l = &g->links[i];
        if (i) sb_append(&sb, ",");
        sb_append(&sb, "{\"id\":\"");
        sb_append_escaped(&sb, l->id);
        sb_append(&sb, "\",\"from_id\":\"");
        sb_append_escaped(&sb, g->nodes[l->from_node].id);
        sb_append(&sb, "\",\"to_id\":\"");
        sb_append_escaped(&sb, g->nodes[l->to_node].id);
        sb_printf(&sb,
            "\",\"length\":%.3f,\"diameter\":%.3f,\"link_type\":\"%s\","
            "\"status\":\"%s\",\"has_valve\":%s,\"disruption_impact\":%.3f}",
            l->length, l->diameter, link_type_str(l->link_type),
            link_status_str(l->status),
            l->has_valve ? "true" : "false",
            l->disruption_impact);
    }
    sb_append(&sb, "]}");
    return sb_take(&sb);
}

/* ── json_reachability ───────────────────────────────────────────── */

char *json_reachability(const Graph *g, const ReachabilityResult *r) {
    SB sb; sb_init(&sb);
    if (!r) { sb_append(&sb, "null"); return sb_take(&sb); }
    sb_printf(&sb, "{\"disconnected_count\":%d,\"disconnected_demand\":%.3f,\"bfs_ms\":%.3f,\"pressure_deficient_count\":%d,\"disconnected_nodes\":[",
              r->disconnected_count, r->disconnected_demand, r->bfs_ms, r->pressure_deficient_count);
    for (int i = 0; i < r->disconnected_count; i++) {
        if (i) sb_append(&sb, ",");
        int ni = r->disconnected_nodes[i];
        sb_printf(&sb, "{\"index\":%d,\"id\":\"", ni);
        sb_append_escaped(&sb, g ? g->nodes[ni].id : "?");
        sb_append(&sb, "\"}");
    }
    sb_append(&sb, "]}");
    return sb_take(&sb);
}

/* ── json_isolation ──────────────────────────────────────────────── */

char *json_isolation(const Graph *g, const IsolationResult *r) {
    SB sb; sb_init(&sb);
    if (!r) { sb_append(&sb, "null"); return sb_take(&sb); }
    sb_append(&sb, "{\"failed_links\":[");
    for (int i = 0; i < r->failed_count; i++) {
        if (i) sb_append(&sb, ",");
        sb_append(&sb, "\"");
        sb_append_escaped(&sb, g->links[r->failed_links[i]].id);
        sb_append(&sb, "\"");
    }
    sb_append(&sb, "],\"valves_to_close\":[");
    for (int i = 0; i < r->valve_count; i++) {
        if (i) sb_append(&sb, ",");
        sb_append(&sb, "\"");
        sb_append_escaped(&sb, g->links[r->valves_to_close[i]].id);
        sb_append(&sb, "\"");
    }
    sb_append(&sb, "],\"extra_disrupted\":[");
    for (int i = 0; i < r->extra_disrupted_count; i++) {
        if (i) sb_append(&sb, ",");
        int ni = r->extra_disrupted[i];
        sb_append(&sb, "\"");
        sb_append_escaped(&sb, g->nodes[ni].id);
        sb_append(&sb, "\"");
    }
    sb_printf(&sb, "],\"total_disrupted_demand\":%.3f,\"isolation_feasible\":%s}",
              r->total_disrupted_demand, r->isolation_feasible ? "true" : "false");
    return sb_take(&sb);
}

/* ── json_structural ─────────────────────────────────────────────── */

char *json_structural(const Graph *g, const StructuralResult *sr) {
    SB sb; sb_init(&sb);
    if (!sr) { sb_append(&sb, "null"); return sb_take(&sb); }
    sb_printf(&sb, "{\"component_count\":%d,\"bridges\":[", sr->component_count);
    for (int i = 0; i < sr->bridge_count; i++) {
        if (i) sb_append(&sb, ",");
        sb_append(&sb, "\"");
        sb_append_escaped(&sb, g->links[sr->bridges[i]].id);
        sb_append(&sb, "\"");
    }
    sb_append(&sb, "],\"articulation_points\":[");
    for (int i = 0; i < sr->ap_count; i++) {
        if (i) sb_append(&sb, ",");
        sb_append(&sb, "\"");
        sb_append_escaped(&sb, g->nodes[sr->articulation_points[i]].id);
        sb_append(&sb, "\"");
    }
    sb_append(&sb, "]}");
    return sb_take(&sb);
}

/* ── json_ranked_links ───────────────────────────────────────────── */

char *json_ranked_links(const Graph *g, const RankedLink *ranked, int count) {
    SB sb; sb_init(&sb);
    sb_append(&sb, "[");
    for (int i = 0; i < count; i++) {
        if (i) sb_append(&sb, ",");
        sb_append(&sb, "{\"link_id\":\"");
        sb_append_escaped(&sb, g->links[ranked[i].link_idx].id);
        sb_printf(&sb, "\",\"score\":%.3f,\"reason\":\"", ranked[i].score);
        sb_append_escaped(&sb, ranked[i].reason);
        sb_append(&sb, "\"}");
    }
    sb_append(&sb, "]");
    return sb_take(&sb);
}

/* ── Helper: serialize one PlacementResult ───────────────────────── */

static void sb_placement(SB *sb, const Graph *g, const PlacementResult *pr) {
    if (!pr) { sb_append(sb, "null"); return; }
    sb_append(sb, "{\"algorithm\":\"");
    sb_append_escaped(sb, pr->algorithm_name);
    sb_printf(sb, "\",\"placed_count\":%d,\"placed_valves\":[", pr->placed_count);
    for (int i = 0; i < pr->placed_count; i++) {
        if (i) sb_append(sb, ",");
        sb_append(sb, "\"");
        sb_append_escaped(sb, g->links[pr->placed_valves[i]].id);
        sb_append(sb, "\"");
    }
    sb_printf(sb,
        "],\"total_disruption\":%.3f,\"baseline_disruption\":%.3f,"
        "\"improvement_pct\":%.2f,\"candidates_evaluated\":%ld,"
        "\"runtime_ms\":%.3f,\"mean_disruption\":%.3f,"
        "\"best_disruption\":%.3f,\"seed\":%u,\"trials\":%d,"
        "\"skipped\":%s,\"skip_reason\":\"",
        pr->total_disruption, pr->baseline_disruption,
        pr->improvement_pct, pr->candidates_evaluated, pr->runtime_ms,
        pr->mean_disruption, pr->best_disruption, pr->seed, pr->trials,
        pr->skipped ? "true" : "false");
    sb_append_escaped(sb, pr->skip_reason);
    sb_append(sb, "\",\"placement_explanations\":[");
    for (int i = 0; i < pr->placed_count; i++) {
        if (i) sb_append(sb, ",");
        sb_printf(sb, "{\"valve\":\""); sb_append_escaped(sb, g->links[pr->placed_valves[i]].id);
        sb_printf(sb, "\",\"before\":%.3f,\"after\":%.3f,\"why\":\"Disruption changed from %.3f to %.3f after placing this valve\"}",
                  pr->placement_before[i], pr->placement_after[i],
                  pr->placement_before[i], pr->placement_after[i]);
    }
    sb_append(sb, "]}");
}

/* ── json_optimization_report ────────────────────────────────────── */

char *json_optimization_report(const Graph *g, const OptimizationReport *report) {
    SB sb; sb_init(&sb);
    if (!report) { sb_append(&sb, "null"); return sb_take(&sb); }
    sb_printf(&sb, "{\"budget\":%d,\"seed\":%u,\"scenario_count\":%d,\"greedy\":",
              report->budget, report->seed, report->scenario_count);
    sb_placement(&sb, g, report->greedy);
    sb_append(&sb, ",\"random\":");
    sb_placement(&sb, g, report->random_pl);
    sb_append(&sb, ",\"topology\":");
    sb_placement(&sb, g, report->topology);
    sb_append(&sb, ",\"exhaustive\":");
    sb_placement(&sb, g, report->exhaustive);
    sb_append(&sb, "}");
    return sb_take(&sb);
}

static bool iso_link_contains(const int *links, int count, int idx) {
    for (int i = 0; i < count; i++) if (links[i] == idx) return true;
    return false;
}
static void sb_id_array(SB *sb, const Graph *g, const int *ids, int count, bool nodes) {
    sb_append(sb, "[");
    for (int i = 0; i < count; i++) { if (i) sb_append(sb, ","); sb_append(sb, "\""); sb_append_escaped(sb, nodes ? g->nodes[ids[i]].id : g->links[ids[i]].id); sb_append(sb, "\""); }
    sb_append(sb, "]");
}
static void sb_scenario_explanation(SB *sb, const Graph *g, const IsolationResult *iso, int scenario_no) {
    bool *reached = calloc((size_t)g->node_count, sizeof(bool)); int *queue = malloc((size_t)(g->node_count > 0 ? g->node_count : 1) * sizeof(int));
    int *route_links = malloc((size_t)(g->link_count > 0 ? g->link_count : 1) * sizeof(int));
    if (!reached || !queue || !route_links) { free(reached); free(queue); free(route_links); sb_append(sb, "null"); return; }
    for (int s = 0; s < g->source_count; s++) { int head = 0, tail = 0, source = g->source_indices[s]; reached[source] = true; queue[tail++] = source;
        while (head < tail) { int u = queue[head++]; for (Edge *e = g->adj[u]; e; e = e->next) {
            if (g->links[e->link_idx].status == STATUS_BURST || g->links[e->link_idx].status == STATUS_CLOSED || iso_link_contains(iso->failed_links, iso->failed_count, e->link_idx) || iso_link_contains(iso->valves_to_close, iso->valve_count, e->link_idx)) continue;
            if (!reached[e->to]) { reached[e->to] = true; queue[tail++] = e->to; }
        }}
    }
    sb_printf(sb, "{\"scenario\":%d,\"failed_links\":", scenario_no); sb_id_array(sb, g, iso->failed_links, iso->failed_count, false);
    sb_append(sb, ",\"valves_to_close\":[");
    for (int i = 0; i < iso->valve_count; i++) { if (i) sb_append(sb, ","); sb_printf(sb, "{\"link\":\""); sb_append_escaped(sb, g->links[iso->valves_to_close[i]].id); sb_printf(sb, "\",\"reason\":\"bounds segment %d with %d node(s)\"}", scenario_no, iso->segment_node_count); }
    sb_append(sb, "],\"stuck_boundary\":[");
    for (int i = 0; i < iso->stuck_boundary_count; i++) { if (i) sb_append(sb, ","); sb_printf(sb, "{\"link\":\""); sb_append_escaped(sb, g->links[iso->stuck_boundary[i]].id); sb_printf(sb, "\",\"reason\":\"stuck, boundary expanded to %d node(s)\"}", iso->segment_node_count); }
    sb_append(sb, "],\"disconnected_junctions\":["); bool first = true;
    for (int i = 0; i < g->node_count; i++) if (!g->nodes[i].is_source && !reached[i]) { if (!first) sb_append(sb, ","); first = false; sb_append(sb, "\""); sb_append_escaped(sb, g->nodes[i].id); sb_append(sb, "\""); }
    sb_append(sb, "],\"alternative_routes\":[");
    for (int s = 0; s < g->source_count; s++) { int head = 0, tail = 0, route_count = 0, source = g->source_indices[s]; bool *seen = calloc((size_t)g->node_count, sizeof(bool)); bool *used = calloc((size_t)g->link_count, sizeof(bool));
        if (!seen || !used) { free(seen); free(used); continue; } seen[source] = true; queue[tail++] = source;
        while (head < tail) { int u = queue[head++]; for (Edge *e = g->adj[u]; e; e = e->next) {
            if (g->links[e->link_idx].status == STATUS_BURST || g->links[e->link_idx].status == STATUS_CLOSED || iso_link_contains(iso->failed_links, iso->failed_count, e->link_idx) || iso_link_contains(iso->valves_to_close, iso->valve_count, e->link_idx)) continue;
            if (!seen[e->to]) { seen[e->to] = true; queue[tail++] = e->to; if (!used[e->link_idx]) { used[e->link_idx] = true; route_links[route_count++] = e->link_idx; } }
        }}
        if (s) sb_append(sb, ",");
        sb_printf(sb, "{\"source\":\""); sb_append_escaped(sb, g->nodes[source].id); sb_append(sb, "\",\"via_links\":"); sb_id_array(sb, g, route_links, route_count, false); sb_append(sb, "}");
        free(seen); free(used);
    }
    sb_printf(sb, "],\"isolation_feasible\":%s}", iso->isolation_feasible ? "true" : "false"); free(reached); free(queue); free(route_links);
}
static char *json_explanations(const Graph *g, const IsolationResult *iso, const OptimizationReport *report) {
    SB sb; sb_init(&sb); sb_append(&sb, "{\"scenarios\":["); int emitted = 0;
    if (report && report->scenarios) for (int i = 0; i < report->scenarios->count; i++) {
        IsolationResult *current = compute_isolation((Graph *)g, report->scenarios->failures[i], report->scenarios->failure_counts[i]);
        if (!current) continue;
        if (emitted++) sb_append(&sb, ",");
        sb_scenario_explanation(&sb, g, current, i + 1);
        free_isolation(current);
    }
    else if (iso) sb_scenario_explanation(&sb, g, iso, 1);
    sb_append(&sb, "]}"); return sb_take(&sb);
}

/* ── json_full_report ────────────────────────────────────────────── */

char *json_full_report(const Graph            *g,
                       const ReachabilityResult *reach,
                       const IsolationResult    *iso,
                       const StructuralResult   *sr,
                       const RankedLink         *ranked, int ranked_count,
                       const OptimizationReport *report,
                       const char                *hydraulics_json) {
    SB sb; sb_init(&sb);
    sb_append(&sb, "{");

    /* Graph topology */
    sb_append(&sb, "\"graph\":");
    char *jg = json_graph(g);
    sb_append(&sb, jg ? jg : "null");
    free(jg);

    /* Reachability */
    sb_append(&sb, ",\"reachability\":");
    char *jr = json_reachability(g, reach);
    sb_append(&sb, jr ? jr : "null");
    free(jr);

    /* Isolation */
    sb_append(&sb, ",\"isolation\":");
    char *ji = json_isolation(g, iso);
    sb_append(&sb, ji ? ji : "null");
    free(ji);

    /* Structural */
    sb_append(&sb, ",\"structural\":");
    char *js = json_structural(g, sr);
    sb_append(&sb, js ? js : "null");
    free(js);

    /* Ranked links */
    sb_append(&sb, ",\"ranked_links\":");
    char *jrl = json_ranked_links(g, ranked, ranked_count);
    sb_append(&sb, jrl ? jrl : "null");
    free(jrl);

    /* Optimization report */
    sb_append(&sb, ",\"optimization\":");
    char *jor = json_optimization_report(g, report);
    sb_append(&sb, jor ? jor : "null");
    free(jor);

    sb_append(&sb, ",\"explanations\":");
    char *je = json_explanations(g, iso, report);
    sb_append(&sb, je ? je : "null");
    free(je);

    sb_printf(&sb, ",\"performance\":{\"bfs_ms\":%.3f,\"optimization_ms\":%.3f,\"memory_kb\":%ld}",
              reach ? reach->bfs_ms : 0.0, report ? report->optimization_ms : 0.0,
              report ? report->memory_kb : -1L);
    sb_append(&sb, ",\"hydraulics\":");
    sb_append(&sb, hydraulics_json ? hydraulics_json : "{\"available\":false,\"note\":\"hydraulic output unavailable\"}");

    sb_append(&sb, "}");
    return sb_take(&sb);
}

/* ── json_write_to_file ──────────────────────────────────────────── */

int json_write_to_file(const char *filepath, const char *json_str) {
    if (!filepath || !json_str) return -1;
    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;
    fputs(json_str, fp);
    fclose(fp);
    return 0;
}

void json_free(char *json_str) {
    free(json_str);
}
