/*
 * inp_parser.c — EPANET .inp file parser implementation
 * Teammate A owns this file.
 */

/* strdup via POSIX on non-Windows */
#if !defined(_WIN32)
#  define _POSIX_C_SOURCE 200809L
#endif

#include "inp_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Portable case-insensitive compare */
#ifdef _WIN32
#  include <string.h>  /* _stricmp */
#  define vw_strcasecmp(a,b) _stricmp((a),(b))
#else
#  include <strings.h>
#  define vw_strcasecmp(a,b) vw_strcasecmp((a),(b))
#endif

#define LINE_BUF 512

/* ── NetworkIndex ──────────────────────────────────────────────────── */

NetworkIndex *network_index_create(void) {
    NetworkIndex *ni = calloc(1, sizeof(NetworkIndex));
    if (!ni) return NULL;
    ni->node_map = hashmap_create(64);
    ni->link_map = hashmap_create(128);
    if (!ni->node_map || !ni->link_map) {
        hashmap_free(ni->node_map);
        hashmap_free(ni->link_map);
        free(ni);
        return NULL;
    }
    return ni;
}

void network_index_free(NetworkIndex *ni) {
    if (!ni) return;
    hashmap_free(ni->node_map);
    hashmap_free(ni->link_map);
    free(ni);
}

/* ── Line utilities ───────────────────────────────────────────────── */

/** Trim trailing whitespace in-place. Return pointer to first non-space char. */
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) *--end = '\0';
    return s;
}

/** Return true if the line is a comment or blank. */
static bool is_skip(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return (*s == ';' || *s == '\0');
}

/** Return true if s starts a new section header [XXX]. */
static bool is_section(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return (*s == '[');
}

/* ── Per-line section parsing ─────────────────────────────────────── */

static void parse_line_junction(const char *line, Graph *g, NetworkIndex *ni) {
    char id[64] = {0};
    double elev = 0.0, demand = 0.0;
    int n = sscanf(line, "%63s %lf %lf", id, &elev, &demand);
    if (n < 1) return;

    int idx = graph_add_node(g, id, NODE_JUNCTION, elev, demand, 0.0, 0.0);
    if (idx >= 0) hashmap_insert(ni->node_map, id, idx);
}

static void parse_line_reservoir(const char *line, Graph *g, NetworkIndex *ni) {
    char id[64] = {0};
    double head = 0.0;
    if (sscanf(line, "%63s %lf", id, &head) < 1) return;

    int idx = graph_add_node(g, id, NODE_RESERVOIR, head, 0.0, 0.0, 0.0);
    if (idx >= 0) hashmap_insert(ni->node_map, id, idx);
}

static void parse_line_tank(const char *line, Graph *g, NetworkIndex *ni) {
    char id[64] = {0};
    double elev = 0.0;
    if (sscanf(line, "%63s %lf", id, &elev) < 1) return;

    int idx = graph_add_node(g, id, NODE_TANK, elev, 0.0, 0.0, 0.0);
    if (idx >= 0) hashmap_insert(ni->node_map, id, idx);
}

static void parse_line_pipe(const char *line, Graph *g, NetworkIndex *ni) {
    char id[64]={0}, n1[64]={0}, n2[64]={0}, status_tok[16]="OPEN";
    double length=0.0, diam=0.0, rough=0.0;

    int n = sscanf(line, "%63s %63s %63s %lf %lf %lf %15s",
                   id, n1, n2, &length, &diam, &rough, status_tok);
    if (n < 6) return;

    int from_idx = -1, to_idx = -1;
    if (!hashmap_get(ni->node_map, n1, &from_idx)) {
        fprintf(stderr, "[PIPES] Unknown node '%s'\n", n1); return;
    }
    if (!hashmap_get(ni->node_map, n2, &to_idx)) {
        fprintf(stderr, "[PIPES] Unknown node '%s'\n", n2); return;
    }

    LinkStatus st = STATUS_OPEN;
    if (vw_strcasecmp(status_tok, "CLOSED") == 0) st = STATUS_CLOSED;

    int idx = graph_add_link(g, id, from_idx, to_idx,
                             length, diam, rough,
                             LINK_TYPE_PIPE, st, false);
    if (idx >= 0) {
        hashmap_insert(ni->link_map, id, idx);
        graph_add_edge(g, from_idx, to_idx, idx);
        graph_add_edge(g, to_idx, from_idx, idx);
    }
}

static void parse_line_valve(const char *line, Graph *g, NetworkIndex *ni) {
    char id[64]={0}, n1[64]={0}, n2[64]={0}, vtype[16]={0};
    double diam=0.0, setting=0.0;

    int n = sscanf(line, "%63s %63s %63s %lf %15s %lf",
                   id, n1, n2, &diam, vtype, &setting);
    if (n < 4) return;

    int from_idx = -1, to_idx = -1;
    if (!hashmap_get(ni->node_map, n1, &from_idx)) return;
    if (!hashmap_get(ni->node_map, n2, &to_idx))   return;

    int idx = graph_add_link(g, id, from_idx, to_idx,
                             0.0, diam, 0.0,
                             LINK_TYPE_VALVE, STATUS_OPEN, true);
    if (idx >= 0) {
        hashmap_insert(ni->link_map, id, idx);
        graph_add_edge(g, from_idx, to_idx, idx);
        graph_add_edge(g, to_idx, from_idx, idx);
    }
}

static void parse_line_pump(const char *line, Graph *g, NetworkIndex *ni) {
    char id[64]={0}, n1[64]={0}, n2[64]={0};
    if (sscanf(line, "%63s %63s %63s", id, n1, n2) < 3) return;

    int from_idx = -1, to_idx = -1;
    if (!hashmap_get(ni->node_map, n1, &from_idx)) return;
    if (!hashmap_get(ni->node_map, n2, &to_idx))   return;

    int idx = graph_add_link(g, id, from_idx, to_idx,
                             100.0, 12.0, 100.0,
                             LINK_TYPE_PUMP, STATUS_OPEN, false);
    if (idx >= 0) {
        hashmap_insert(ni->link_map, id, idx);
        graph_add_edge(g, from_idx, to_idx, idx);
        graph_add_edge(g, to_idx, from_idx, idx);
    }
}

static void parse_line_coordinate(const char *line, Graph *g, NetworkIndex *ni) {
    char id[64] = {0};
    double x = 0.0, y = 0.0;
    if (sscanf(line, "%63s %lf %lf", id, &x, &y) < 3) return;

    int idx = -1;
    if (hashmap_get(ni->node_map, id, &idx)) {
        g->nodes[idx].x = x;
        g->nodes[idx].y = y;
    }
}

static void parse_line_demand(const char *line, Graph *g, NetworkIndex *ni) {
    char id[64]={0}, pattern[64]={0};
    double demand = 0.0;
    if (sscanf(line, "%63s %lf %63s", id, &demand, pattern) < 2) return;

    int idx = -1;
    if (hashmap_get(ni->node_map, id, &idx))
        g->nodes[idx].base_demand += demand;
}

/* ── Section parsers (retained for backward compatibility) ────────── */

int parse_section_junctions(FILE *fp, Graph *g, NetworkIndex *ni)   { (void)fp; (void)g; (void)ni; return 0; }
int parse_section_reservoirs(FILE *fp, Graph *g, NetworkIndex *ni)  { (void)fp; (void)g; (void)ni; return 0; }
int parse_section_tanks(FILE *fp, Graph *g, NetworkIndex *ni)       { (void)fp; (void)g; (void)ni; return 0; }
int parse_section_pipes(FILE *fp, Graph *g, NetworkIndex *ni)       { (void)fp; (void)g; (void)ni; return 0; }
int parse_section_valves(FILE *fp, Graph *g, NetworkIndex *ni)      { (void)fp; (void)g; (void)ni; return 0; }
int parse_section_coordinates(FILE *fp, Graph *g, NetworkIndex *ni) { (void)fp; (void)g; (void)ni; return 0; }
int parse_section_demands(FILE *fp, Graph *g, NetworkIndex *ni)     { (void)fp; (void)g; (void)ni; return 0; }

/* ── parse_inp_file (clean state-machine parsing without seek) ────── */

typedef enum {
    SEC_NONE = 0,
    SEC_JUNCTIONS,
    SEC_RESERVOIRS,
    SEC_TANKS,
    SEC_PIPES,
    SEC_VALVES,
    SEC_PUMPS,
    SEC_COORDINATES,
    SEC_DEMANDS,
    SEC_END
} InpSection;

int parse_inp_file(const char *filepath, Graph *g, NetworkIndex *ni) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) { fprintf(stderr, "Cannot open '%s'\n", filepath); return -1; }

    char buf[LINE_BUF];
    InpSection current_sec = SEC_NONE;

    while (fgets(buf, sizeof(buf), fp)) {
        char *line = trim(buf);
        if (is_skip(line)) continue;

        if (is_section(line)) {
            char sec_name[64] = {0};
            sscanf(line, "[%63[^]]", sec_name);
            for (char *p = sec_name; *p; p++) *p = (char)toupper((unsigned char)*p);

            if      (strcmp(sec_name, "JUNCTIONS")   == 0) current_sec = SEC_JUNCTIONS;
            else if (strcmp(sec_name, "RESERVOIRS")  == 0) current_sec = SEC_RESERVOIRS;
            else if (strcmp(sec_name, "TANKS")       == 0) current_sec = SEC_TANKS;
            else if (strcmp(sec_name, "PIPES")       == 0) current_sec = SEC_PIPES;
            else if (strcmp(sec_name, "VALVES")      == 0) current_sec = SEC_VALVES;
            else if (strcmp(sec_name, "PUMPS")       == 0) current_sec = SEC_PUMPS;
            else if (strcmp(sec_name, "COORDINATES") == 0) current_sec = SEC_COORDINATES;
            else if (strcmp(sec_name, "DEMANDS")     == 0) current_sec = SEC_DEMANDS;
            else if (strcmp(sec_name, "END")         == 0) break;
            else                                           current_sec = SEC_NONE;
            continue;
        }

        switch (current_sec) {
            case SEC_JUNCTIONS:   parse_line_junction(line, g, ni);   break;
            case SEC_RESERVOIRS:  parse_line_reservoir(line, g, ni);  break;
            case SEC_TANKS:       parse_line_tank(line, g, ni);       break;
            case SEC_PIPES:       parse_line_pipe(line, g, ni);       break;
            case SEC_VALVES:      parse_line_valve(line, g, ni);      break;
            case SEC_PUMPS:       parse_line_pump(line, g, ni);       break;
            case SEC_COORDINATES: parse_line_coordinate(line, g, ni); break;
            case SEC_DEMANDS:     parse_line_demand(line, g, ni);     break;
            default: break;
        }
    }

    fclose(fp);
    return 0;
}
