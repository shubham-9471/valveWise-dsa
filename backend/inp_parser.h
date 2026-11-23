/*
 * inp_parser.h — EPANET-style .inp file parser for ValveWise
 * Teammate A owns this file.
 */

#ifndef INP_PARSER_H
#define INP_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "graph.h"
#include "hashmap.h"

/** Holds two hash maps: one for node IDs and one for link IDs. */
typedef struct {
    HashMap *node_map; /**< string node ID → index in Graph.nodes[] */
    HashMap *link_map; /**< string link ID → index in Graph.links[] */
} NetworkIndex;

/** Allocate a fresh NetworkIndex with empty hash maps. */
NetworkIndex *network_index_create(void);

/** Free all memory owned by ni. */
void network_index_free(NetworkIndex *ni);

/**
 * parse_inp_file — open and parse an EPANET .inp file into g and ni.
 * @return 0 on success, -1 on file/parse error.
 */
int parse_inp_file(const char *filepath, Graph *g, NetworkIndex *ni);

/* Individual section parsers (exposed for unit testing) */
int parse_section_junctions   (FILE *fp, Graph *g, NetworkIndex *ni);
int parse_section_reservoirs  (FILE *fp, Graph *g, NetworkIndex *ni);
int parse_section_tanks       (FILE *fp, Graph *g, NetworkIndex *ni);
int parse_section_pipes       (FILE *fp, Graph *g, NetworkIndex *ni);
int parse_section_valves      (FILE *fp, Graph *g, NetworkIndex *ni);
int parse_section_coordinates (FILE *fp, Graph *g, NetworkIndex *ni);
int parse_section_demands     (FILE *fp, Graph *g, NetworkIndex *ni);

#ifdef __cplusplus
}
#endif

#endif /* INP_PARSER_H */
