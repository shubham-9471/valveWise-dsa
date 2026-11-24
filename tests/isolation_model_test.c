#include "graph.h"
#include "isolation.h"
#include <stdio.h>
#include <stdlib.h>

static int failures;
static unsigned seed = 0xA2C0DEu;
static unsigned rnd(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static Graph *new_graph(int n) {
    Graph *g = graph_create(n + 2, n * 3 + 4); if (!g) return NULL;
    graph_add_node(g, "R", NODE_RESERVOIR, 0, 0, 0, 0);
    for (int i = 1; i < n; i++) { char id[8]; snprintf(id, sizeof(id), "J%d", i); graph_add_node(g, id, NODE_JUNCTION, 0, i, 0, 0); }
    return g;
}
static int link_add(Graph *g, int a, int b, int type, LinkStatus status, int valve) {
    char id[8]; snprintf(id, sizeof(id), "L%d", g->link_count);
    int li = graph_add_link(g, id, a, b, 1, 1, 1, type, status, valve != 0);
    graph_add_edge(g, a, b, li); graph_add_edge(g, b, a, li); return li;
}
static void check(int ok, const char *name) { if (!ok) { fprintf(stderr, "FAIL: %s\n", name); failures++; } }

static void hand_tests(void) {
    int f; Graph *g = new_graph(4);
    link_add(g, 0, 1, LINK_TYPE_PIPE, STATUS_OPEN, 1); f = link_add(g, 1, 2, LINK_TYPE_PIPE, STATUS_OPEN, 0);
    link_add(g, 2, 3, LINK_TYPE_PIPE, STATUS_OPEN, 1); link_add(g, 3, 0, LINK_TYPE_PIPE, STATUS_OPEN, 0);
    IsolationResult *r = compute_isolation(g, &f, 1);
    check(r && r->isolation_feasible && r->valve_count == 2 && r->total_disrupted_demand == 3, "ring"); free_isolation(r); graph_free(g);

    g = new_graph(4); link_add(g, 0, 1, LINK_TYPE_PIPE, STATUS_OPEN, 0); link_add(g, 1, 2, LINK_TYPE_PIPE, STATUS_OPEN, 1);
    f = link_add(g, 2, 3, LINK_TYPE_PIPE, STATUS_OPEN, 0); r = compute_isolation(g, &f, 1);
    check(r && r->isolation_feasible && r->total_disrupted_demand == 5, "tree bridge"); free_isolation(r); graph_free(g);

    g = new_graph(2); f = link_add(g, 0, 1, LINK_TYPE_PIPE, STATUS_OPEN, 0); r = compute_isolation(g, &f, 1);
    check(r && !r->isolation_feasible && r->total_disrupted_demand == 1, "source inside segment"); free_isolation(r); graph_free(g);

    g = new_graph(4); link_add(g, 0, 1, LINK_TYPE_PIPE, STATUS_OPEN, 1); f = link_add(g, 1, 2, LINK_TYPE_PIPE, STATUS_OPEN, 0);
    link_add(g, 1, 3, LINK_TYPE_PIPE, STATUS_STUCK, 1); link_add(g, 3, 0, LINK_TYPE_PIPE, STATUS_OPEN, 1); r = compute_isolation(g, &f, 1);
    check(r && r->isolation_feasible && r->total_disrupted_demand == 6 && r->extra_disrupted_count == 1, "stuck valve"); free_isolation(r); graph_free(g);

    g = new_graph(3); link_add(g, 0, 1, LINK_TYPE_PIPE, STATUS_OPEN, 0); link_add(g, 0, 1, LINK_TYPE_PIPE, STATUS_OPEN, 0);
    f = link_add(g, 1, 2, LINK_TYPE_PIPE, STATUS_OPEN, 0); StructuralResult *s = compute_structural_analysis(g);
    check(s && s->bridge_count == 1 && s->bridges[0] == f, "parallel pipes Tarjan"); free_structural(s); graph_free(g);
}

static void property_test(void) {
    for (int trial = 0; trial < 1000; trial++) {
        int n = 3 + (int)(rnd() % 5); Graph *g = new_graph(n);
        for (int i = 1; i < n; i++) link_add(g, i - 1, i, LINK_TYPE_PIPE, STATUS_OPEN, 0);
        for (int i = 0, extra = (int)(rnd() % (n + 2)); i < extra; i++) {
            int a = (int)(rnd() % n), b = (int)(rnd() % n); if (a == b) b = (b + 1) % n;
            link_add(g, a, b, LINK_TYPE_PIPE, STATUS_OPEN, 0);
        }
        int failed = (int)(rnd() % g->link_count), candidate = failed == 0 ? 1 : 0;
        IsolationResult *before = compute_isolation(g, &failed, 1); g->links[candidate].has_valve = true;
        IsolationResult *after = compute_isolation(g, &failed, 1);
        check(before && after && after->total_disrupted_demand <= before->total_disrupted_demand + 1e-9, "adding non-stuck valve monotonicity");
        free_isolation(before); free_isolation(after); graph_free(g);
    }
}
int main(void) { hand_tests(); property_test(); if (failures) return 1; puts("A2 isolation tests: 1000 seeded property cases + hand checks passed"); return 0; }
