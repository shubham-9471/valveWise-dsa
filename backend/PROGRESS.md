# PROGRESS.md — anti/backend
- Done M1/M2: AGENTS/PROGRESS and portable C build/strdup/strcasecmp fixes.
- Done M3/M4: LF `.gitattributes` and root Makefile targets.
- Done M5/M6: integration runner and removed EPANET-compat claim.
- Done A2: isolation segment model, JSON contract field, fixed Tarjan parallel edges.
- Done A2 tests: seeded 1000-case monotonicity plus ring/tree/source/stuck/parallel checks.
- Done A3: shared scenarios/evaluator, comparable algorithms, seeded random stats, exhaustive cap.
- Done A3 tests: optimizer ordering/baseline properties on 100 small graphs.
## Next
- Commit A3 to anti/backend and push.
## Decisions
- Segments cross unvalved/stuck links; failed links are removed; source-containing segments cost total demand.
- Stuck extra disruption is measured against an operable-valve counterfactual; total uses actual stuck result.
- Tarjan tracks parent link index, not parent node, so parallel pipes remain back edges.
- A3: scenarios default to single non-pump failures; `--fail` is one multi-failure scenario; candidates exclude valves/pumps; exhaustive caps at 500,000.
