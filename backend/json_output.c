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

/* ═══════════════════════════════════════════════════════════════════
 * Growable string buffer
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} SB;

static void sb_init(SB *sb) {
    sb->buf = calloc(4096, 1);
    sb->cap = 4096;
    sb->len = 0;
}

static void sb_grow(SB *sb, size_t needed) {
    while (sb->len + needed >= sb->cap) {
        sb->cap *= 2;
        char *tmp = realloc(sb->buf, sb->cap);
        if (!tmp) return; /* OOM: silently truncate */
        sb->buf = tmp;
    }
}

static void sb_append(SB *sb, const char *s) {
    size_t slen = strlen(s);
    sb_grow(sb, slen + 1);
    memcpy(sb->buf + sb->len, s, slen);
    sb->len += slen;
    sb->buf[sb->len] = '\0';
}

static void sb_printf(SB *sb, const char *fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    sb_append(sb, tmp);
}

static char *sb_take(SB *sb) {
    /* Caller owns the returned string; free with json_free() */
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
        sb_printf(&sb,
            "{\"id\":\"%s\",\"type\":\"%s\",\"elevation\":%.3f,"
            "\"base_demand\":%.3f,\"x\":%.3f,\"y\":%.3f,\"is_source\":%s}",
            n->id, node_type_str(n->type), n->elevation,
            n->base_demand, n->x, n->y,
            n->is_source ? "true" : "false");
    }
    sb_append(&sb, "],\"links\":[");
    for (int i = 0; i < g->link_count; i++) {
        const Link *l = &g->links[i];
        if (i) sb_append(&sb, ",");
        sb_printf(&sb,
            "{\"id\":\"%s\",\"from_id\":\"%s\",\"to_id\":\"%s\","
            "\"length\":%.3f,\"diameter\":%.3f,\"link_type\":\"%s\","
            "\"status\":\"%s\",\"has_valve\":%s,\"disruption_impact\":%.3f}",
            l->id,
            g->nodes[l->from_node].id, g->nodes[l->to_node].id,
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
    sb_printf(&sb, "{\"disconnected_count\":%d,\"disconnected_demand\":%.3f,\"disconnected_nodes\":[",
              r->disconnected_count, r->disconnected_demand);
    for (int i = 0; i < r->disconnected_count; i++) {
        if (i) sb_append(&sb, ",");
        int ni = r->disconnected_nodes[i];
        sb_printf(&sb, "{\"index\":%d,\"id\":\"%s\"}", ni, g ? g->nodes[ni].id : "?");
    }
    sb_append(&sb, "]}");
    return sb_take(&sb);
}

/* ── json_isolation ──────────────────────────────────────────────── */

char *json_isolation(const Graph *g, const IsolationResult *r) {
    SB sb; sb_init(&sb);
    if (!r) { sb_append(&sb, "null"); return sb_take(&sb); }
    sb_printf(&sb, "{\"failed_links\":[");
    for (int i = 0; i < r->failed_count; i++) {
        if (i) sb_append(&sb, ",");
        sb_printf(&sb, "\"%s\"", g->links[r->failed_links[i]].id);
    }
    sb_append(&sb, "],\"valves_to_close\":[");
    for (int i = 0; i < r->valve_count; i++) {
        if (i) sb_append(&sb, ",");
        sb_printf(&sb, "\"%s\"", g->links[r->valves_to_close[i]].id);
    }
    sb_append(&sb, "],\"extra_disrupted\":[");
    for (int i = 0; i < r->extra_disrupted_count; i++) {
        if (i) sb_append(&sb, ",");
        int ni = r->extra_disrupted[i];
        sb_printf(&sb, "\"%s\"", g->nodes[ni].id);
    }
    sb_printf(&sb, "],\"total_disrupted_demand\":%.3f}", r->total_disrupted_demand);
    return sb_take(&sb);
}

/* ── json_structural ─────────────────────────────────────────────── */

char *json_structural(const Graph *g, const StructuralResult *sr) {
    SB sb; sb_init(&sb);
    if (!sr) { sb_append(&sb, "null"); return sb_take(&sb); }
    sb_printf(&sb, "{\"component_count\":%d,\"bridges\":[", sr->component_count);
    for (int i = 0; i < sr->bridge_count; i++) {
        if (i) sb_append(&sb, ",");
        sb_printf(&sb, "\"%s\"", g->links[sr->bridges[i]].id);
    }
    sb_append(&sb, "],\"articulation_points\":[");
    for (int i = 0; i < sr->ap_count; i++) {
        if (i) sb_append(&sb, ",");
        sb_printf(&sb, "\"%s\"", g->nodes[sr->articulation_points[i]].id);
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
        sb_printf(&sb, "{\"link_id\":\"%s\",\"score\":%.3f,\"reason\":\"%s\"}",
                  g->links[ranked[i].link_idx].id,
                  ranked[i].score,
                  ranked[i].reason);
    }
    sb_append(&sb, "]");
    return sb_take(&sb);
}

/* ── Helper: serialize one PlacementResult ───────────────────────── */

static void sb_placement(SB *sb, const Graph *g, const PlacementResult *pr) {
    if (!pr) { sb_append(sb, "null"); return; }
    sb_printf(sb,
        "{\"algorithm\":\"%s\",\"placed_count\":%d,\"placed_valves\":[",
        pr->algorithm_name, pr->placed_count);
    for (int i = 0; i < pr->placed_count; i++) {
        if (i) sb_append(sb, ",");
        sb_printf(sb, "\"%s\"", g->links[pr->placed_valves[i]].id);
    }
    sb_printf(sb,
        "],\"total_disruption\":%.3f,\"baseline_disruption\":%.3f,"
        "\"improvement_pct\":%.2f,\"candidates_evaluated\":%ld,"
        "\"runtime_ms\":%.3f}",
        pr->total_disruption, pr->baseline_disruption,
        pr->improvement_pct, pr->candidates_evaluated, pr->runtime_ms);
}

/* ── json_optimization_report ────────────────────────────────────── */

char *json_optimization_report(const Graph *g, const OptimizationReport *report) {
    SB sb; sb_init(&sb);
    if (!report) { sb_append(&sb, "null"); return sb_take(&sb); }
    sb_printf(&sb, "{\"budget\":%d,\"greedy\":", report->budget);
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

/* ── json_full_report ────────────────────────────────────────────── */

char *json_full_report(const Graph            *g,
                       const ReachabilityResult *reach,
                       const IsolationResult    *iso,
                       const StructuralResult   *sr,
                       const RankedLink         *ranked, int ranked_count,
                       const OptimizationReport *report) {
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
