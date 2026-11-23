/*
 * optimizer.h — Valve placement optimization & critical component ranking
 * ValveWise — Teammate B owns this file.
 */

#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "graph.h"
#include "heap.h"
#include "isolation.h"
#include <stdbool.h>

/* ── Ranking mode ───────────────────────────────────────────────────── */

typedef enum {
    RANK_BY_DISRUPTION  = 0, /**< Demand disconnected on simulated failure */
    RANK_BY_BETWEENNESS = 1, /**< Bridge-based heuristic (highest for bridges) */
    RANK_BY_DEGREE      = 2  /**< Score = degree(from) × degree(to) */
} RankMode;

/* ── Ranked link ────────────────────────────────────────────────────── */

typedef struct {
    int    link_idx;        /**< Index into Graph.links[] */
    double score;           /**< Higher = more critical */
    char   reason[256];     /**< Human-readable explanation */
} RankedLink;

/* ── Placement result for one algorithm ────────────────────────────── */

typedef struct {
    int    *placed_valves;        /**< Link indices where NEW valves are recommended */
    int     placed_count;
    double  total_disruption;     /**< Demand-weighted disruption with these valves (L/s) */
    double  baseline_disruption;  /**< Disruption with NO extra valves */
    double  improvement_pct;      /**< 100 × (baseline − total) / baseline */
    long    candidates_evaluated; /**< Number of candidate sets tested */
    double  runtime_ms;           /**< Wall-clock time (milliseconds) */
    char    algorithm_name[64];
} PlacementResult;

/* Full optimization comparison */
typedef struct {
    PlacementResult *greedy;
    PlacementResult *random_pl;
    PlacementResult *topology;
    PlacementResult *exhaustive; /**< NULL if graph is too large */
    int              budget;
} OptimizationReport;

/* Function prototypes */
RankedLink      *rank_links(const Graph *g,
                           const StructuralResult *sr,
                           RankMode mode,
                           int *out_count);

double compute_total_disruption(Graph *g,
                                const int *valve_links, int valve_count,
                                const int *failure_scenarios, int scenario_count);

PlacementResult *optimize_greedy(Graph *g, int budget,
                                  const int *failure_scenarios, int scenario_count);

PlacementResult *optimize_random(Graph *g, int budget,
                                  const int *failure_scenarios, int scenario_count,
                                  unsigned int seed);

PlacementResult *optimize_topology(Graph *g, int budget,
                                    const StructuralResult *sr);

PlacementResult *optimize_exhaustive(Graph *g, int budget,
                                      const int *failure_scenarios, int scenario_count);

OptimizationReport *run_full_optimization(Graph *g, int budget,
                                           const int *failure_scenarios, int scenario_count,
                                           const StructuralResult *sr,
                                           unsigned int seed);

void free_placement_result(PlacementResult *pr);
void free_optimization_report(OptimizationReport *report);

#ifdef __cplusplus
}
#endif

#endif /* OPTIMIZER_H */
