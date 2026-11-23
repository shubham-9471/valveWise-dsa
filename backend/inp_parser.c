/*
 * inp_parser.c — EPANET .inp file parser implementation
 * Teammate A owns this file.
 */

#include "inp_parser.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

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

/* ── Section parsers ──────────────────────────────────────────────── */

/*
 * Each parser reads lines until it hits the next section header or EOF.
 * It does NOT consume the section header line — parse_inp_file handles that.
 * FILE* is positioned just after the section header when these are called.
 */

int parse_section_junctions(FILE *fp, Graph *g, NetworkIndex *ni) {
    char buf[LINE_BUF];
    long pos;
    while ((pos = ftell(fp)) >= 0 && fgets(buf, sizeof(buf), fp)) {
        char *line = trim(buf);
        if (is_section(line)) { fseek(fp, pos, SEEK_SET); break; }
        if (is_skip(line)) continue;

        char id[64] = {0};
        double elev = 0.0, demand = 0.0;
        int n = sscanf(line, "%63s %lf %lf", id, &elev, &demand);
        if (n < 1) continue;

        int idx = graph_add_node(g, id, NODE_JUNCTION, elev, demand, 0.0, 0.0);
        if (idx >= 0) hashmap_insert(ni->node_map, id, idx);
    }
    return 0;
}

int parse_section_reservoirs(FILE *fp, Graph *g, NetworkIndex *ni) {
    char buf[LINE_BUF];
    long pos;
    while ((pos = ftell(fp)) >= 0 && fgets(buf, sizeof(buf), fp)) {
        char *line = trim(buf);
        if (is_section(line)) { fseek(fp, pos, SEEK_SET); break; }
        if (is_skip(line)) continue;

        char id[64] = {0};
        double head = 0.0;
        if (sscanf(line, "%63s %lf", id, &head) < 1) continue;

        int idx = graph_add_node(g, id, NODE_RESERVOIR, head, 0.0, 0.0, 0.0);
        if (idx >= 0) hashmap_insert(ni->node_map, id, idx);
    }
    return 0;
}

int parse_section_tanks(FILE *fp, Graph *g, NetworkIndex *ni) {
    char buf[LINE_BUF];
    long pos;
    while ((pos = ftell(fp)) >= 0 && fgets(buf, sizeof(buf), fp)) {
        char *line = trim(buf);
        if (is_section(line)) { fseek(fp, pos, SEEK_SET); break; }
        if (is_skip(line)) continue;

        char id[64] = {0};
        double elev = 0.0, minlvl = 0.0, maxlvl = 0.0, initlvl = 0.0, diam = 0.0;
        if (sscanf(line, "%63s %lf %lf %lf %lf %lf",
                   id, &elev, &minlvl, &maxlvl, &initlvl, &diam) < 1) continue;

        int idx = graph_add_node(g, id, NODE_TANK, elev, 0.0, 0.0, 0.0);
        if (idx >= 0) hashmap_insert(ni->node_map, id, idx);
    }
    return 0;
}

int parse_section_pipes(FILE *fp, Graph *g, NetworkIndex *ni) {
    char buf[LINE_BUF];
    long pos;
    while ((pos = ftell(fp)) >= 0 && fgets(buf, sizeof(buf), fp)) {
        char *line = trim(buf);
        if (is_section(line)) { fseek(fp, pos, SEEK_SET); break; }
        if (is_skip(line)) continue;

        char id[64]={0}, n1[64]={0}, n2[64]={0}, status_tok[16]="OPEN";
        double length=0.0, diam=0.0, rough=0.0;

        int n = sscanf(line, "%63s %63s %63s %lf %lf %lf %15s",
                       id, n1, n2, &length, &diam, &rough, status_tok);
        if (n < 6) continue;

        int from_idx = -1, to_idx = -1;
        if (!hashmap_get(ni->node_map, n1, &from_idx)) {
            fprintf(stderr, "[PIPES] Unknown node '%s'\n", n1); continue;
        }
        if (!hashmap_get(ni->node_map, n2, &to_idx)) {
            fprintf(stderr, "[PIPES] Unknown node '%s'\n", n2); continue;
        }

        LinkStatus st = STATUS_OPEN;
        if (strcasecmp(status_tok, "CLOSED") == 0) st = STATUS_CLOSED;

        int idx = graph_add_link(g, id, from_idx, to_idx,
                                 length, diam, rough,
                                 LINK_TYPE_PIPE, st, false);
        if (idx >= 0) {
            hashmap_insert(ni->link_map, id, idx);
            graph_add_edge(g, from_idx, to_idx, idx);
            graph_add_edge(g, to_idx, from_idx, idx);
        }
    }
    return 0;
}

int parse_section_valves(FILE *fp, Graph *g, NetworkIndex *ni) {
    char buf[LINE_BUF];
    long pos;
    while ((pos = ftell(fp)) >= 0 && fgets(buf, sizeof(buf), fp)) {
        char *line = trim(buf);
        if (is_section(line)) { fseek(fp, pos, SEEK_SET); break; }
        if (is_skip(line)) continue;

        char id[64]={0}, n1[64]={0}, n2[64]={0}, vtype[16]={0};
        double diam=0.0, setting=0.0;

        int n = sscanf(line, "%63s %63s %63s %lf %15s %lf",
                       id, n1, n2, &diam, vtype, &setting);
        if (n < 4) continue;

        int from_idx = -1, to_idx = -1;
        if (!hashmap_get(ni->node_map, n1, &from_idx)) continue;
        if (!hashmap_get(ni->node_map, n2, &to_idx))   continue;

        int idx = graph_add_link(g, id, from_idx, to_idx,
                                 0.0, diam, 0.0,
                                 LINK_TYPE_VALVE, STATUS_OPEN, true);
        if (idx >= 0) {
            hashmap_insert(ni->link_map, id, idx);
            graph_add_edge(g, from_idx, to_idx, idx);
            graph_add_edge(g, to_idx, from_idx, idx);
        }
    }
    return 0;
}

int parse_section_coordinates(FILE *fp, Graph *g, NetworkIndex *ni) {
    char buf[LINE_BUF];
    long pos;
    while ((pos = ftell(fp)) >= 0 && fgets(buf, sizeof(buf), fp)) {
        char *line = trim(buf);
        if (is_section(line)) { fseek(fp, pos, SEEK_SET); break; }
        if (is_skip(line)) continue;

        char id[64] = {0};
        double x = 0.0, y = 0.0;
        if (sscanf(line, "%63s %lf %lf", id, &x, &y) < 3) continue;

        int idx = -1;
        if (hashmap_get(ni->node_map, id, &idx)) {
            g->nodes[idx].x = x;
            g->nodes[idx].y = y;
        }
    }
    return 0;
}

int parse_section_demands(FILE *fp, Graph *g, NetworkIndex *ni) {
    char buf[LINE_BUF];
    long pos;
    while ((pos = ftell(fp)) >= 0 && fgets(buf, sizeof(buf), fp)) {
        char *line = trim(buf);
        if (is_section(line)) { fseek(fp, pos, SEEK_SET); break; }
        if (is_skip(line)) continue;

        char id[64]={0}, pattern[64]={0};
        double demand = 0.0;
        if (sscanf(line, "%63s %lf %63s", id, &demand, pattern) < 2) continue;

        int idx = -1;
        if (hashmap_get(ni->node_map, id, &idx))
            g->nodes[idx].base_demand += demand;
    }
    return 0;
}

/* ── parse_inp_file ─────────────────────────────────────────────── */

int parse_inp_file(const char *filepath, Graph *g, NetworkIndex *ni) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) { fprintf(stderr, "Cannot open '%s'\n", filepath); return -1; }

    char buf[LINE_BUF];
    while (fgets(buf, sizeof(buf), fp)) {
        char *line = trim(buf);
        if (is_skip(line)) continue;

        if (!is_section(line)) continue; /* Skip stray text before first section */

        /* Extract section name */
        char section[64] = {0};
        sscanf(line, "[%63[^]]", section);
        for (char *p = section; *p; p++) *p = (char)toupper((unsigned char)*p);

        if      (strcmp(section, "JUNCTIONS")   == 0) parse_section_junctions(fp, g, ni);
        else if (strcmp(section, "RESERVOIRS")  == 0) parse_section_reservoirs(fp, g, ni);
        else if (strcmp(section, "TANKS")       == 0) parse_section_tanks(fp, g, ni);
        else if (strcmp(section, "PIPES")       == 0) parse_section_pipes(fp, g, ni);
        else if (strcmp(section, "VALVES")      == 0) parse_section_valves(fp, g, ni);
        else if (strcmp(section, "COORDINATES") == 0) parse_section_coordinates(fp, g, ni);
        else if (strcmp(section, "DEMANDS")     == 0) parse_section_demands(fp, g, ni);
        else if (strcmp(section, "END")         == 0) break;
        /* Ignore all other sections silently */
    }

    fclose(fp);
    return 0;
}
