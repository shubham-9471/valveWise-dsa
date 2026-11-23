/*
 * graph.c — Graph ADT implementation for ValveWise
 * Teammate A owns this file.
 */

#include "graph.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal helpers ──────────────────────────────────────────────── */

/** Double node capacity: reallocate nodes[] and adj[]. */
static int grow_nodes(Graph *g) {
    int new_cap = g->node_capacity * 2;

    Node *new_nodes = realloc(g->nodes, (size_t)new_cap * sizeof(Node));
    if (!new_nodes) return -1;
    g->nodes = new_nodes;

    Edge **new_adj = realloc(g->adj, (size_t)new_cap * sizeof(Edge *));
    if (!new_adj) return -1;
    g->adj = new_adj;

    /* Initialise the new slots to NULL */
    for (int i = g->node_capacity; i < new_cap; i++)
        g->adj[i] = NULL;

    g->node_capacity = new_cap;
    return 0;
}

/** Double link capacity: reallocate links[]. */
static int grow_links(Graph *g) {
    int new_cap = g->link_capacity * 2;
    Link *new_links = realloc(g->links, (size_t)new_cap * sizeof(Link));
    if (!new_links) return -1;
    g->links = new_links;
    g->link_capacity = new_cap;
    return 0;
}

/** Double source_indices capacity. */
static int grow_sources(Graph *g) {
    int new_cap = g->source_capacity * 2;
    int *new_src = realloc(g->source_indices, (size_t)new_cap * sizeof(int));
    if (!new_src) return -1;
    g->source_indices = new_src;
    g->source_capacity = new_cap;
    return 0;
}

/* ── graph_create ──────────────────────────────────────────────────── */

Graph *graph_create(int initial_node_cap, int initial_link_cap) {
    if (initial_node_cap <= 0) initial_node_cap = 16;
    if (initial_link_cap <= 0) initial_link_cap = 32;

    Graph *g = calloc(1, sizeof(Graph));
    if (!g) return NULL;

    g->nodes = calloc((size_t)initial_node_cap, sizeof(Node));
    g->links = calloc((size_t)initial_link_cap, sizeof(Link));
    g->adj   = calloc((size_t)initial_node_cap, sizeof(Edge *));
    g->source_indices = calloc(8, sizeof(int));

    if (!g->nodes || !g->links || !g->adj || !g->source_indices) {
        free(g->nodes); free(g->links); free(g->adj);
        free(g->source_indices); free(g);
        return NULL;
    }

    g->node_capacity   = initial_node_cap;
    g->link_capacity   = initial_link_cap;
    g->source_capacity = 8;
    g->node_count      = 0;
    g->link_count      = 0;
    g->source_count    = 0;
    return g;
}

/* ── graph_free ────────────────────────────────────────────────────── */

void graph_free(Graph *g) {
    if (!g) return;

    /* Free each adjacency linked list */
    for (int i = 0; i < g->node_capacity; i++) {
        Edge *e = g->adj[i];
        while (e) {
            Edge *next = e->next;
            free(e);
            e = next;
        }
    }
    free(g->adj);
    free(g->nodes);
    free(g->links);
    free(g->source_indices);
    free(g);
}

/* ── graph_add_node ────────────────────────────────────────────────── */

int graph_add_node(Graph *g, const char *id, NodeType type,
                   double elevation, double demand, double x, double y) {
    if (!g || !id) return -1;

    /* Grow if needed */
    if (g->node_count >= g->node_capacity) {
        if (grow_nodes(g) != 0) return -1;
    }

    int idx = g->node_count;
    Node *n = &g->nodes[idx];

    strncpy(n->id, id, NODE_ID_LEN - 1);
    n->id[NODE_ID_LEN - 1] = '\0';
    n->type        = type;
    n->elevation   = elevation;
    n->base_demand = demand;
    n->x           = x;
    n->y           = y;
    n->is_source   = (type == NODE_RESERVOIR || type == NODE_TANK);

    /* Register as a source */
    if (n->is_source) {
        if (g->source_count >= g->source_capacity) {
            if (grow_sources(g) != 0) return -1;
        }
        g->source_indices[g->source_count++] = idx;
    }

    g->adj[idx] = NULL; /* Initialise adjacency head */
    g->node_count++;
    return idx;
}

/* ── graph_add_link ────────────────────────────────────────────────── */

int graph_add_link(Graph *g, const char *id, int from, int to,
                   double length, double diameter, double roughness,
                   int link_type, LinkStatus status, bool has_valve) {
    if (!g || !id) return -1;
    if (from < 0 || from >= g->node_count) return -1;
    if (to   < 0 || to   >= g->node_count) return -1;

    if (g->link_count >= g->link_capacity) {
        if (grow_links(g) != 0) return -1;
    }

    int idx = g->link_count;
    Link *l = &g->links[idx];

    strncpy(l->id, id, LINK_ID_LEN - 1);
    l->id[LINK_ID_LEN - 1] = '\0';
    l->from_node       = from;
    l->to_node         = to;
    l->length          = length;
    l->diameter        = diameter;
    l->roughness       = roughness;
    l->link_type       = link_type;
    l->status          = status;
    l->has_valve       = has_valve;
    l->disruption_impact = 0.0;

    g->link_count++;
    return idx;
}

/* ── graph_add_edge ────────────────────────────────────────────────── */

void graph_add_edge(Graph *g, int from, int to, int link_idx) {
    if (!g || from < 0 || from >= g->node_count) return;

    Edge *e = malloc(sizeof(Edge));
    if (!e) return;

    e->to       = to;
    e->link_idx = link_idx;
    e->next     = g->adj[from];  /* Prepend */
    g->adj[from] = e;
}

/* ── graph_print_summary ───────────────────────────────────────────── */

void graph_print_summary(const Graph *g) {
    if (!g) { printf("graph_print_summary: NULL graph\n"); return; }
    printf("Graph summary: %d nodes, %d links, %d sources\n",
           g->node_count, g->link_count, g->source_count);
}
