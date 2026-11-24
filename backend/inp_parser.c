/* Robust, dynamically-sized EPANET-style .inp parser. */
#include "inp_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

NetworkIndex *network_index_create(void) {
    NetworkIndex *ni = calloc(1, sizeof(*ni));
    if (!ni) return NULL;
    ni->node_map = hashmap_create(64); ni->link_map = hashmap_create(128);
    if (!ni->node_map || !ni->link_map) { network_index_free(ni); return NULL; }
    return ni;
}
void network_index_free(NetworkIndex *ni) {
    if (!ni) return;
    hashmap_free(ni->node_map); hashmap_free(ni->link_map); free(ni);
}
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = '\0'; return s;
}
static int equal_ci(const char *a, const char *b) {
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
    return *a == '\0' && *b == '\0';
}
static int read_line(FILE *fp, char **out) {
    size_t len = 0, cap = 256; int c; char *line = malloc(cap); if (!line) return -1;
    while ((c = fgetc(fp)) != EOF) {
        if (len + 1 >= cap) {
            if (cap > SIZE_MAX / 2) { free(line); return -1; }
            cap *= 2; char *tmp = realloc(line, cap);
            if (!tmp) { free(line); return -1; }
            line = tmp;
        }
        line[len++] = (char)c; if (c == '\n') break;
    }
    if (len == 0 && c == EOF) { free(line); return 0; }
    line[len] = '\0'; *out = line; return 1;
}
static void strip_comment(char *s) { char *p = strchr(s, ';'); if (p) *p = '\0'; }
static int node_index(NetworkIndex *ni, const char *id, int *out) { return hashmap_get(ni->node_map, id, out); }
static int add_node(Graph *g, NetworkIndex *ni, const char *id, NodeType type, double a, double demand) {
    int old; if (hashmap_get(ni->node_map, id, &old)) return -1;
    int idx = graph_add_node(g, id, type, a, demand, 0, 0); if (idx < 0) return -1;
    return hashmap_insert(ni->node_map, id, idx) ? idx : -1;
}
static int add_link(Graph *g, NetworkIndex *ni, const char *id, const char *a, const char *b,
                    double length, double diameter, double roughness, int type, LinkStatus status, bool valve) {
    int from, to, old;
    if (hashmap_get(ni->link_map, id, &old) || !node_index(ni, a, &from) || !node_index(ni, b, &to)) return -1;
    int idx = graph_add_link(g, id, from, to, length, diameter, roughness, type, status, valve);
    if (idx < 0 || !hashmap_insert(ni->link_map, id, idx)) return -1;
    graph_add_edge(g, from, to, idx); graph_add_edge(g, to, from, idx); return idx;
}
static int parse_junction(char *line, Graph *g, NetworkIndex *ni) {
    char id[64]; double elev, demand; return sscanf(line, "%63s %lf %lf", id, &elev, &demand) == 3 && add_node(g, ni, id, NODE_JUNCTION, elev, demand) >= 0;
}
static int parse_reservoir(char *line, Graph *g, NetworkIndex *ni) {
    char id[64]; double head; return sscanf(line, "%63s %lf", id, &head) == 2 && add_node(g, ni, id, NODE_RESERVOIR, head, 0) >= 0;
}
static int parse_tank(char *line, Graph *g, NetworkIndex *ni) {
    char id[64]; double elev; return sscanf(line, "%63s %lf", id, &elev) >= 2 && add_node(g, ni, id, NODE_TANK, elev, 0) >= 0;
}
static int parse_pipe(char *line, Graph *g, NetworkIndex *ni) {
    char id[64], a[64], b[64], status[16] = "OPEN"; double l, d, r;
    int n = sscanf(line, "%63s %63s %63s %lf %lf %lf %15s", id, a, b, &l, &d, &r, status);
    return n >= 6 && add_link(g, ni, id, a, b, l, d, r, LINK_TYPE_PIPE, equal_ci(status, "CLOSED") ? STATUS_CLOSED : STATUS_OPEN, false) >= 0;
}
static int parse_pump(char *line, Graph *g, NetworkIndex *ni) {
    char id[64], a[64], b[64]; return sscanf(line, "%63s %63s %63s", id, a, b) == 3 && add_link(g, ni, id, a, b, 100, 12, 100, LINK_TYPE_PUMP, STATUS_OPEN, false) >= 0;
}
static int parse_valve(char *line, Graph *g, NetworkIndex *ni) {
    char id[64], a[64], b[64], type[16]; double d, setting; int n = sscanf(line, "%63s %63s %63s %lf %15s %lf", id, a, b, &d, type, &setting);
    return n >= 4 && add_link(g, ni, id, a, b, 0, d, 0, LINK_TYPE_VALVE, STATUS_OPEN, true) >= 0;
}
static int parse_coordinate(char *line, Graph *g, NetworkIndex *ni) {
    char id[64]; double x, y; int idx; return sscanf(line, "%63s %lf %lf", id, &x, &y) == 3 && node_index(ni, id, &idx) && ((g->nodes[idx].x = x), (g->nodes[idx].y = y), 1);
}
static int parse_demand(char *line, Graph *g, NetworkIndex *ni) {
    char id[64], pattern[64]; double demand; int idx; int n = sscanf(line, "%63s %lf %63s", id, &demand, pattern);
    return n >= 2 && node_index(ni, id, &idx) && ((g->nodes[idx].base_demand += demand), 1);
}
int parse_section_junctions(FILE *f, Graph *g, NetworkIndex *n) { (void)f; (void)g; (void)n; return 0; }
int parse_section_reservoirs(FILE *f, Graph *g, NetworkIndex *n) { (void)f; (void)g; (void)n; return 0; }
int parse_section_tanks(FILE *f, Graph *g, NetworkIndex *n) { (void)f; (void)g; (void)n; return 0; }
int parse_section_pipes(FILE *f, Graph *g, NetworkIndex *n) { (void)f; (void)g; (void)n; return 0; }
int parse_section_pumps(FILE *f, Graph *g, NetworkIndex *n) { (void)f; (void)g; (void)n; return 0; }
int parse_section_valves(FILE *f, Graph *g, NetworkIndex *n) { (void)f; (void)g; (void)n; return 0; }
int parse_section_coordinates(FILE *f, Graph *g, NetworkIndex *n) { (void)f; (void)g; (void)n; return 0; }
int parse_section_demands(FILE *f, Graph *g, NetworkIndex *n) { (void)f; (void)g; (void)n; return 0; }

typedef enum { SEC_NONE, SEC_JUNCTIONS, SEC_RESERVOIRS, SEC_TANKS, SEC_PIPES, SEC_PUMPS, SEC_VALVES, SEC_COORDINATES, SEC_DEMANDS } Section;
static Section section_for(const char *name) {
    if (equal_ci(name, "JUNCTIONS")) return SEC_JUNCTIONS;
    if (equal_ci(name, "RESERVOIRS")) return SEC_RESERVOIRS;
    if (equal_ci(name, "TANKS")) return SEC_TANKS;
    if (equal_ci(name, "PIPES")) return SEC_PIPES;
    if (equal_ci(name, "PUMPS")) return SEC_PUMPS;
    if (equal_ci(name, "VALVES")) return SEC_VALVES;
    if (equal_ci(name, "COORDINATES")) return SEC_COORDINATES;
    if (equal_ci(name, "DEMANDS")) return SEC_DEMANDS;
    return SEC_NONE;
}
int parse_inp_file(const char *filepath, Graph *g, NetworkIndex *ni) {
    if (!filepath || !g || !ni) return -1;
    FILE *fp = fopen(filepath, "r"); if (!fp) return -1;
    Section sec = SEC_NONE; int records = 0, line_no = 0, rc; char *raw = NULL;
    while ((rc = read_line(fp, &raw)) > 0) {
        line_no++; strip_comment(raw); char *line = trim(raw);
        if (!*line) { free(raw); raw = NULL; continue; }
        if (*line == '[') {
            char name[64] = {0}; char *end = strchr(line, ']');
            if (!end || sscanf(line + 1, "%63[^]]", name) != 1) { free(raw); fclose(fp); return -1; }
            if (equal_ci(name, "END")) { free(raw); raw = NULL; break; }
            sec = section_for(trim(name)); free(raw); raw = NULL; continue;
        }
        int ok = 1;
        switch (sec) {
            case SEC_JUNCTIONS: ok = parse_junction(line, g, ni); break; case SEC_RESERVOIRS: ok = parse_reservoir(line, g, ni); break;
            case SEC_TANKS: ok = parse_tank(line, g, ni); break; case SEC_PIPES: ok = parse_pipe(line, g, ni); break;
            case SEC_PUMPS: ok = parse_pump(line, g, ni); break; case SEC_VALVES: ok = parse_valve(line, g, ni); break;
            case SEC_COORDINATES: ok = parse_coordinate(line, g, ni); break; case SEC_DEMANDS: ok = parse_demand(line, g, ni); break;
            default: ok = 1; break;
        }
        if (sec != SEC_NONE && !ok) { fprintf(stderr, "Parse error at line %d\n", line_no); free(raw); fclose(fp); return -1; }
        if (sec != SEC_NONE) records++;
        free(raw); raw = NULL;
    }
    free(raw); fclose(fp); return (rc < 0 || records == 0 || g->node_count == 0 || g->link_count == 0) ? -1 : 0;
}
