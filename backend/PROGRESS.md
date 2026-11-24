# PROGRESS.md — anti/backend
- Done M1-M6/A2-A6: portable build, isolation, optimizer, parser/data, explanations/benchmarks, iterative Tarjan hardening, fuzz and scale checks.
- Done H1: vendored OWA-EPANET 2.2 PDA states, optional CLI hydraulics JSON, MinGW build, and hand checks.
## Next
- Commit H1 to anti/backend and push.
## Decisions
- Decisions: segments cross unvalved/stuck links; source segments cost total demand; stuck extra uses an operable counterfactual; Tarjan keys parent edges for parallel pipes.
- A3/A4: shared non-pump scenarios/candidates, exhaustive cap 500k; dynamic parser, external valves, generators, fixtures, and Net1/Net3 data.
- A5/A6: measured benchmarks; MinGW lacks ASan/UBSan libraries and Valgrind; normal, fuzz, property, hand, and scale checks pass.
- H1: EPANET runs four independent states outside optimization; `--hydraulics off` preserves all prior analysis fields; C++ stand-in remains demo-only.
