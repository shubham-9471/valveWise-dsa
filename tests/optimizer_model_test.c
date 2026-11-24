#include "graph.h"
#include "isolation.h"
#include "optimizer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned state = 0xA30001u;
static unsigned rnd(void) { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
static int add_link(Graph *g, int a, int b) {
    char id[8]; snprintf(id, sizeof(id), "P%d", g->link_count);
    int i = graph_add_link(g, id, a, b, 1, 1, 1, LINK_TYPE_PIPE, STATUS_OPEN, false);
    graph_add_edge(g, a, b, i); graph_add_edge(g, b, a, i); return i;
}
static Graph *make_graph(int n) {
    Graph *g = graph_create(n + 2, n * 3 + 4); if (!g) return NULL;
    graph_add_node(g, "R", NODE_RESERVOIR, 0, 0, 0, 0);
    for (int i = 1; i < n; i++) { char id[32]; snprintf(id, sizeof(id), "J%d", i); graph_add_node(g, id, NODE_JUNCTION, 0, i, 0, 0); }
    for (int i = 1; i < n; i++) add_link(g, i - 1, i);
    for (int i = 0; i < (int)(rnd() % (unsigned)n); i++) { int a = rnd() % n, b = rnd() % n; if (a != b) add_link(g, a, b); }
    return g;
}
int main(void) {
    for (int t = 0; t < 100; t++) {
        Graph *g = make_graph(3 + (int)(rnd() % 4));
        ScenarioSet *s = scenario_set_all(g); StructuralResult *sr = compute_structural_analysis(g);
        OptimizationReport *r = run_full_optimization(g, 2, s, sr, 77);
        if (!r || !r->greedy || !r->random_pl || !r->topology || !r->exhaustive || r->exhaustive->skipped ||
            fabs(r->greedy->baseline_disruption - r->random_pl->baseline_disruption) > 1e-9 ||
            fabs(r->greedy->baseline_disruption - r->topology->baseline_disruption) > 1e-9 ||
            fabs(r->greedy->baseline_disruption - r->exhaustive->baseline_disruption) > 1e-9 ||
            r->exhaustive->total_disruption > r->greedy->total_disruption + 1e-9 ||
            r->greedy->total_disruption > r->greedy->baseline_disruption + 1e-9 ||
            r->random_pl->total_disruption > r->random_pl->baseline_disruption + 1e-9 ||
            r->topology->total_disruption > r->topology->baseline_disruption + 1e-9 ||
            r->random_pl->seed != 77) return 1;
        free_optimization_report(r); free_structural(sr); free_scenario_set(s); graph_free(g);
    }
    puts("A3 optimizer properties: 100 small graphs passed"); return 0;
}
