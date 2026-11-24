# PROGRESS.md — anti/backend
- Done M1-M6: backend rules, portable C fixes, line endings, Makefiles, tests, and README scope.
- Done A2: isolation segment model, JSON field, Tarjan parallel-edge fix; 1000 seeded cases pass.
- Done A3: shared evaluator, comparable algorithms, seeded random stats, exhaustive cap; 100 graph properties pass.
- Done A4: robust dynamic parser, external valves, generators, hand fixtures, and Net1/Net3 data.
## Next
- Commit A4 to anti/backend and push.
## Decisions
- Segments cross unvalved/stuck links; failed links are removed; source-containing segments cost total demand.
- Stuck extra disruption is measured against an operable-valve counterfactual; total uses actual stuck result.
- Tarjan tracks parent link index, not parent node, so parallel pipes remain back edges.
- A3: scenarios default to single non-pump failures; `--fail` is one multi-failure scenario; candidates exclude valves/pumps; exhaustive caps at 500,000.
- A4: unknown sections are skipped; known malformed records, empty networks, and garbage inputs fail with JSON errors.
- A4: `.inp` files remain EPANET-valid; isolation valves live in companion files and generated valves use seeded degree>=3 selection.
