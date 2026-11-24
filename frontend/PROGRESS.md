done
- Created branch codex/frontend and saved frontend rules.
- Built backend in a temp directory without editing backend files.
- Generated four engine JSON fixtures.
- Added adapter, tests, and fixture/upload summary UI.
- Added interactive SVG network view with state styling, tooltips, pan/zoom, fit, and legend.
- Added pure graph style mapping tests; all 4 tests pass.
- Added zero-dependency server API with validation, timeout, temp-file cleanup, and static frontend serving.
- Added simulation controls, network upload, run/reset, error banner, and offline demo fallback.
- Verified API network listing, validation errors, and missing-binary guidance.
- Added loading skeletons, empty states, responsive tablet rules, focus styles, ARIA labels, and keyboard graph activation.
- Preserved backend error JSON and added malformed/partial/empty/extra-field adapter tests.
- Replaced per-element graph listeners with delegated events; 2000-node layout measured at 3.6 ms.
- Added frontend README and five-minute viva DEMO script.
- Added adapter-fed impact, isolation, ranking, optimization, and performance panels.
- Integrated isolation feasibility, per-scenario explanations, optimization means/bests/placement reasons, performance timings, and backend error JSON.
- Regenerated eleven fixtures from the updated backend, including external-valve and Net1/Net3 benchmark outputs.
- Server now materializes API valve IDs as a temporary valves file and auto-loads bundled network valves.
- Added graph highlighting for affected/extra items, ranking focus, and before/after proposals.
- Added honest optimization comparison with negative/zero improvements and skip reasons.
- Added tolerant frozen-schema hydraulics normalization and a hand-authored hydraulics demo fixture.
- Added hydraulic state selector, topology/pressure layer toggle, pressure color/flow-width styling, and distinct deficiency/disconnection marks.
- Added hydraulic state comparison panel and server validation/pass-through for hydraulics mode and minimum pressure.
next
- Commit and push frontend/server milestone if origin is available.
decisions
- Summary demand uses isolation.total_disrupted_demand, falling back to reachability.disconnected_demand.
