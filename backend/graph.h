/*
 * graph.h — Core graph data structures for ValveWise
 * Water Distribution Network Failure & Resilience Optimizer
 *
 * Teammate A owns this file.
 * Defines Node, Link, Edge (adjacency list) and the main Graph struct.
 */

#ifndef GRAPH_H
#define GRAPH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════
 * Status enumerations
 * ═══════════════════════════════════════════════════════════════════ */

/** Operational status of a pipe or valve link */
typedef enum {
    STATUS_OPEN   = 0,  /**< Link is open and operational */
    STATUS_CLOSED = 1,  /**< Link is manually closed */
    STATUS_BURST  = 2,  /**< Pipe has burst / failed */
    STATUS_STUCK  = 3   /**< Valve is stuck-open (cannot be closed) */
} LinkStatus;

/** Type of a network node */
typedef enum {
    NODE_JUNCTION   = 0,  /**< Demand junction */
    NODE_RESERVOIR  = 1,  /**< Infinite source reservoir */
    NODE_TANK       = 2   /**< Finite-volume storage tank (also a source) */
} NodeType;

/* ═══════════════════════════════════════════════════════════════════
 * Adjacency-list edge
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Edge — one directed edge in the adjacency list.
 * The graph stores two Edge records per undirected link (from→to and to→from).
 */
typedef struct Edge {
    int          to;        /**< Destination node index (into Graph.nodes[]) */
    int          link_idx;  /**< Index of the corresponding Link in Graph.links[] */
    struct Edge *next;      /**< Next edge in the singly-linked adjacency list */
} Edge;

/* ═══════════════════════════════════════════════════════════════════
 * Node (junction / reservoir / tank)
 * ═══════════════════════════════════════════════════════════════════ */

#define NODE_ID_LEN 64  /**< Maximum length of a string node ID */

/** A single network node parsed from [JUNCTIONS], [RESERVOIRS], or [TANKS] */
typedef struct {
    char     id[NODE_ID_LEN]; /**< String ID from .inp file (e.g. "J1", "R1") */
    NodeType type;            /**< Junction, Reservoir, or Tank */
    double   elevation;       /**< Elevation in metres */
    double   base_demand;     /**< Base demand in L/s (0 for sources) */
    double   x;               /**< Canvas x-coordinate (from [COORDINATES]) */
    double   y;               /**< Canvas y-coordinate (from [COORDINATES]) */
    bool     is_source;       /**< True for Reservoir and Tank nodes */
} Node;

/* ═══════════════════════════════════════════════════════════════════
 * Link (pipe or valve)
 * ═══════════════════════════════════════════════════════════════════ */

#define LINK_TYPE_PIPE  0  /**< Ordinary pipe */
#define LINK_TYPE_VALVE 1  /**< Isolation / control valve */
#define LINK_TYPE_PUMP  2  /**< Booster / inline pump */

#define LINK_ID_LEN 64  /**< Maximum length of a string link ID */

/** A single pipe or valve link parsed from [PIPES] or [VALVES] */
typedef struct {
    char       id[LINK_ID_LEN]; /**< String ID from .inp file (e.g. "P5", "V1") */
    int        from_node;       /**< Index of start node in Graph.nodes[] */
    int        to_node;         /**< Index of end node in Graph.nodes[] */
    double     length;          /**< Pipe length in metres */
    double     diameter;        /**< Pipe diameter in mm */
    double     roughness;       /**< Hazen-Williams C or Darcy-Weisbach e */
    int        link_type;       /**< LINK_TYPE_PIPE or LINK_TYPE_VALVE */
    LinkStatus status;          /**< Current operational status */
    bool       has_valve;       /**< True if an isolation valve exists on this link */
    double     disruption_impact; /**< Cached: total demand disconnected if this link fails (L/s) */
} Link;

/* ═══════════════════════════════════════════════════════════════════
 * Main Graph structure
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Graph — the entire water distribution network.
 *
 * Memory layout:
 *   nodes[]  — dynamic array; grows by doubling
 *   links[]  — dynamic array; grows by doubling
 *   adj[]    — array of linked-list heads (one per node slot)
 */
typedef struct {
    /* Nodes */
    Node *nodes;         /**< Dynamic array of Node structs */
    int   node_count;    /**< Number of valid nodes currently in the array */
    int   node_capacity; /**< Allocated capacity of nodes[] */

    /* Links */
    Link *links;         /**< Dynamic array of Link structs */
    int   link_count;    /**< Number of valid links currently in the array */
    int   link_capacity; /**< Allocated capacity of links[] */

    /* Adjacency lists */
    Edge **adj;          /**< adj[i] = head of edge list for node i; size = node_capacity */

    /* Source nodes (reservoirs + tanks) */
    int   source_count;    /**< Number of source nodes */
    int  *source_indices;  /**< Dynamic array of source node indices */
    int   source_capacity; /**< Allocated capacity of source_indices[] */
} Graph;

/* ═══════════════════════════════════════════════════════════════════
 * Function prototypes
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * graph_create — allocate and initialise an empty Graph.
 * @param initial_node_cap  starting capacity for nodes[] and adj[]
 * @param initial_link_cap  starting capacity for links[]
 * @return pointer to new Graph, or NULL on allocation failure
 */
Graph *graph_create(int initial_node_cap, int initial_link_cap);

/**
 * graph_free — release all memory owned by g (including g itself).
 */
void graph_free(Graph *g);

/**
 * graph_add_node — append a node to the graph.
 * Reallocates nodes[] and adj[] when capacity is exceeded (doubling strategy).
 * @return index of the newly added node, or -1 on error
 */
int graph_add_node(Graph *g,
                   const char *id,
                   NodeType    type,
                   double      elevation,
                   double      demand,
                   double      x,
                   double      y);

/**
 * graph_add_link — append a link (pipe or valve) to the graph.
 * Does NOT add adjacency edges; call graph_add_edge separately.
 * @return index of the newly added link, or -1 on error
 */
int graph_add_link(Graph     *g,
                   const char *id,
                   int         from,
                   int         to,
                   double      length,
                   double      diameter,
                   double      roughness,
                   int         link_type,
                   LinkStatus  status,
                   bool        has_valve);

/**
 * graph_add_edge — prepend a directed edge from→to (referencing link_idx)
 * into the adjacency list.  Call twice (swapping from/to) for undirected links.
 */
void graph_add_edge(Graph *g, int from, int to, int link_idx);

/** Print a one-line summary of node count, link count, and source count. */
void graph_print_summary(const Graph *g);

#ifdef __cplusplus
}
#endif

#endif /* GRAPH_H */
