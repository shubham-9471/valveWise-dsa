/*
 * isolation.h — Pipe failure simulation & isolation boundary analysis
 * ValveWise — Teammate B owns this file.
 *
 * Provides:
 *   - BFS-based reachability from active water sources
 *   - Tarjan's algorithm for bridge/articulation-point detection
 *   - Isolation boundary computation (which valves to close)
 */

#ifndef ISOLATION_H
#define ISOLATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "graph.h"
#include "queue.h"
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════
 * Reachability result
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    bool   *reachable;            /**< reachable[i] = true iff node i can receive water */
    int     disconnected_count;   /**< Number of nodes that cannot receive water */
    double  disconnected_demand;  /**< Sum of base_demand for all unreachable nodes (L/s) */
    int    *disconnected_nodes;   /**< Array of unreachable node indices */
} ReachabilityResult;

/* ═══════════════════════════════════════════════════════════════════
 * Isolation result
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    int    *failed_links;          /**< Indices of burst/failed links (input copy) */
    int     failed_count;
    int    *valves_to_close;       /**< Link indices of operable valves to close */
    int     valve_count;
    int    *extra_disrupted;       /**< Node indices additionally disrupted by stuck-open valves */
    int     extra_disrupted_count;
    double  total_disrupted_demand;/**< Total demand disrupted (L/s) after isolation */
    bool    isolation_feasible;     /**< Final segment contains no source node */
} IsolationResult;

/* ═══════════════════════════════════════════════════════════════════
 * Structural analysis result (DFS / Tarjan)
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct {
    int *component_id;         /**< component_id[i] = which component node i belongs to */
    int  component_count;
    int *bridges;              /**< Link indices that are bridges (removal disconnects graph) */
    int  bridge_count;
    int *articulation_points;  /**< Node indices that are articulation points */
    int  ap_count;
} StructuralResult;

/* ═══════════════════════════════════════════════════════════════════
 * Function prototypes
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * compute_reachability — BFS from all source nodes.
 * Skips edges where the referenced link is STATUS_BURST or STATUS_CLOSED.
 */
ReachabilityResult *compute_reachability(const Graph *g);
void                free_reachability(ReachabilityResult *r);

/**
 * compute_isolation — determine which valves to close to isolate failed pipes.
 * Handles stuck-open valves by expanding the boundary and recording extra disruption.
 */
IsolationResult *compute_isolation(Graph *g,
                                   const int *failed_links,
                                   int        failed_count);
void             free_isolation(IsolationResult *r);

/**
 * compute_structural_analysis — DFS to find connected components, bridges,
 * and articulation points via Tarjan's bridge-finding algorithm.
 */
StructuralResult *compute_structural_analysis(const Graph *g);
void              free_structural(StructuralResult *sr);

/** Mark a single link as STATUS_BURST. */
void simulate_failure(Graph *g, int link_idx);

/** Reset all STATUS_BURST links back to STATUS_OPEN. */
void reset_failures(Graph *g);

#ifdef __cplusplus
}
#endif

#endif /* ISOLATION_H */
