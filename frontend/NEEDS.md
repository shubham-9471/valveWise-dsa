# Backend coordination

- The fetched `origin/main` currently contains only the earlier isolation contract entry; the full A3-A6 backend contract/data are on `origin/anti/backend` at `9995266`. Fixtures were regenerated from that verified backend commit. Merge that backend history into main before relying on a main-only checkout.
- The backend CLI expects `--valves` to receive a file path, not a comma-separated ID list. The server materializes validated API IDs into a temporary file and records the distinction here rather than hiding it.
- F1 includes `fixtures/hydraulics-demo.json`, a hand-authored frozen-schema fallback until backend hydraulics output is merged.
