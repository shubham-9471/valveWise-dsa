# PROGRESS.md — anti/backend
- Done M1: AGENTS/PROGRESS; M2: portable C build/strdup/strcasecmp fixes.
- Done M3: LF `.gitattributes`; M4: root Makefile targets.
- Done M5: integration test runner; M6: removed EPANET-compat claim from README.
- Done A2: isolation segment model, JSON contract field, fixed Tarjan parallel edges.
- Done A2 tests: seeded 1000-case monotonicity plus ring/tree/source/stuck/parallel checks.
## Next
- Commit A2 to anti/backend and push.
## Decisions
- Segments cross unvalved and stuck links only; failed links are removed.
- Operable boundary valves are the plan; source-containing segments are infeasible and cost total demand.
- Stuck extra disruption is measured against an operable-valve counterfactual; total uses actual stuck result.
- Tarjan tracks parent link index, not parent node, so parallel pipes remain back edges.
- A1: `make all` builds C backend; `make hydraulic` builds C++ simulator.
