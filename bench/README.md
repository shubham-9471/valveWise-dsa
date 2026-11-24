# Benchmark method
Networks are sample, net_image3, ring6, grid4x4, tree_bridge, Net1, and Net3.
Each is run with budgets 1, 2, 3, and 5 using all single non-pump failures.
Greedy, topology, and exhaustive use seed 1000 once; random uses seeds 1000-1004.
`run.sh` writes the measured matrix to `bench/results.csv`.
The CSV records baseline, final disruption, improvement percentage, runtime, and candidates.
Findings below are generated only after running the matrix.

## Findings
Measured matrix: 7 networks x 4 budgets; deterministic algorithms have 28 rows and random has 140.
Exhaustive had the best mean improvement at 11.65%.
Random was second at 11.44%, only 0.20 percentage points behind exhaustive.
Greedy averaged 7.52% improvement.
Topology averaged 6.39% improvement.
Exhaustive strictly beat greedy in 7 of 28 cases and matched it in the remaining cases.
No exhaustive case hit the 500,000-combination cap in this matrix.
The average greedy-minus-exhaustive final-disruption gap was 273.500 L/s.
Random timings include 100 internal placements per trial and five seeded trials per cell.
Per-cell baselines, final disruption, runtimes, and candidate counts are in `results.csv`.
