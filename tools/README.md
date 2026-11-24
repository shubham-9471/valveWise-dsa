# Backend data tools

`gen_valves.py` computes endpoint degree from `[PIPES]`, considers a pipe
eligible when either endpoint has degree >= 3, and selects eligible pipes with
the requested density using a local RNG seeded by `--seed`. One selected pipe
ID is written per line; append `stuck` to mark it stuck-open. `gen_grid.py`
creates connected 1,000- or 10,000-node grid inputs for scale tests.
