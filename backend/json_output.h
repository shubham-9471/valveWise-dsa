/*
 * json_output.h — JSON serialization for ValveWise analysis results
 * Teammate B owns this file.
 */

#ifndef JSON_OUTPUT_H
#define JSON_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "graph.h"
#include "isolation.h"
#include "optimizer.h"

/** Serialize the full graph topology (nodes + links) as a JSON string. */
char *json_graph(const Graph *g);

/** Serialize a reachability result. */
char *json_reachability(const Graph *g, const ReachabilityResult *r);

/** Serialize an isolation result. */
char *json_isolation(const Graph *g, const IsolationResult *r);

/** Serialize structural analysis (components, bridges, articulation points). */
char *json_structural(const Graph *g, const StructuralResult *sr);

/** Serialize a ranked-link array (sorted by score). */
char *json_ranked_links(const Graph *g, const RankedLink *ranked, int count);

/** Serialize a full optimization report (all four algorithms). */
char *json_optimization_report(const Graph *g, const OptimizationReport *report);

/**
 * json_full_report — compose all results into one top-level JSON object.
 * Any pointer may be NULL; that key is emitted as null.
 */
char *json_full_report(const Graph            *g,
                       const ReachabilityResult *reach,
                       const IsolationResult    *iso,
                       const StructuralResult   *sr,
                       const RankedLink         *ranked, int ranked_count,
                       const OptimizationReport *report);

/** Write a JSON string to filepath.  Returns 0 on success, -1 on error. */
int   json_write_to_file(const char *filepath, const char *json_str);

/** Free a string returned by any json_* function. */
void  json_free(char *json_str);

#ifdef __cplusplus
}
#endif

#endif /* JSON_OUTPUT_H */
