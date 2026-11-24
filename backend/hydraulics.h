#ifndef VALVEWISE_HYDRAULICS_H
#define VALVEWISE_HYDRAULICS_H
#include "graph.h"
#include "optimizer.h"
typedef struct { char *json; int pressure_deficient_count; } HydraulicsOutput;
HydraulicsOutput *hydraulics_run(const char *, const Graph *, const int *, int, const int *, int, const PlacementResult *, int, double, bool);
void hydraulics_free(HydraulicsOutput *);
#endif
