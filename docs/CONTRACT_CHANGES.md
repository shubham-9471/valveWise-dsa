# docs/CONTRACT_CHANGES.md — Additive JSON output changes log
# Format: json_path | type | meaning | date

# isolation.isolation_feasible | bool | Whether the final failure segment can be isolated without containing a source | 2026-11-24
# optimization.seed | uint | Seed used by random placement trials | 2026-11-24
# optimization.scenario_count | int | Number of shared failure scenarios evaluated by all algorithms | 2026-11-24
# optimization.*.mean_disruption/best_disruption/trials/skipped/skip_reason | number/int/bool/string | Comparable random statistics and exhaustive cap status | 2026-11-24
