# docs/CONTRACT_CHANGES.md — Additive JSON output changes log
# Format: json_path | type | meaning | date

# isolation.isolation_feasible | bool | Whether the final failure segment can be isolated without containing a source | 2026-11-24
# optimization.seed | uint | Seed used by random placement trials | 2026-11-24
# optimization.scenario_count | int | Number of shared failure scenarios evaluated by all algorithms | 2026-11-24
# optimization.*.mean_disruption/best_disruption/trials/skipped/skip_reason | number/int/bool/string | Comparable random statistics and exhaustive cap status | 2026-11-24
# CLI --valves FILE | flag | Loads external one-link-per-line isolation valves; optional second token `stuck` | 2026-11-24
# explanations.scenarios | array | Per-scenario generated valve reasons, stuck-boundary reasons, disconnected junctions, source routes, and feasibility | 2026-11-24
# optimization.*.placement_explanations | array | Per-placed-valve generated disruption before/after values and plain-English reason | 2026-11-24
# reachability.bfs_ms/performance | number/object | BFS, optimization, and peak-memory timing fields (`memory_kb` is -1 when unavailable) | 2026-11-24
