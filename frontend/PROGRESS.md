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
next
- Commit and push frontend/server milestone if origin is available.
decisions
- Summary demand uses isolation.total_disrupted_demand, falling back to reachability.disconnected_demand.
