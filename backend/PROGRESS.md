# PROGRESS.md — anti/backend
- Done M1-M6: backend rules, portable C fixes, line endings, Makefiles, tests, and README scope.
- Done A2: isolation segment model, JSON field, Tarjan parallel-edge fix; 1000 seeded cases pass.
- Done A3: shared evaluator, comparable algorithms, seeded random stats, exhaustive cap; 100 graph properties pass.
- Done A4: robust dynamic parser, external valves, generators, hand fixtures, and Net1/Net3 data.
- Done A5: generated explanations, performance metrics, and measured benchmark CSV/findings.
## Next
- Commit A5 to anti/backend and push.
## Decisions
- Segments cross unvalved/stuck links; failed links are removed; source-containing segments cost total demand.
- Stuck extra disruption is measured against an operable-valve counterfactual; total uses actual stuck result.
- Tarjan tracks parent link index, not parent node, so parallel pipes remain back edges.
- A3: scenarios default to single non-pump failures; `--fail` is one multi-failure scenario; candidates exclude valves/pumps; exhaustive caps at 500,000.
- A4: unknown sections skip; malformed/empty/garbage inputs fail; `.inp` stays EPANET-valid and valves are external seeded degree>=3 selections.
- A5: benchmark findings are measured from 7 networks x 4 budgets; random uses five seeds per cell.
