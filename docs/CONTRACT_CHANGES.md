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
# hydraulics | object | Optional real OWA-EPANET 2.2 PDA results; includes availability/note, units, threshold, and four graph-aligned states | 2026-11-24
# hydraulics.*.nodes | array | Per-node pressure, computed demand, and served flag for baseline, after_failure, after_isolation, and after_optimization | 2026-11-24
# hydraulics.*.links | array | Per-link EPANET flow and status for each hydraulic state | 2026-11-24
# hydraulics.*.pressure_deficient/unserved_demand | array/number | Junction IDs below `min_pressure` and demand not served relative to baseline | 2026-11-24
# reachability.pressure_deficient_count | int | Pressure-deficient junction count from the after-failure hydraulic state, or zero when unavailable | 2026-11-24
# CLI --hydraulics on|off | flag | Enables/disables EPANET output; default on | 2026-11-24
# CLI --min-pressure N | flag | PDA minimum pressure threshold in psi; default 20 | 2026-11-24
