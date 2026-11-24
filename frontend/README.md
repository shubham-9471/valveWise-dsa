# ValveWise frontend

## Install and run

There are no npm dependencies or build steps. From the repository root, build the backend and start the server:

```text
make -C backend
node server/server.js
```

Open `http://localhost:3000`. Use `PORT=8080` for another port or `VALVEWISE_BIN` for a custom binary. A missing binary returns a clear `make -C backend` hint.

## Folder map

- `index.html`, `styles.css`, `theme.css`, `controls.css`: responsive accessible shell and theme.
- `js/adapter.js`: the only backend-JSON reader; normalizes missing, additive, malformed, and unknown fields.
- `js/graphView.js`: dependency-free SVG graph, delegated keyboard/pointer events, and cached layout.
- `js/panels.js`: impact, isolation, ranking, optimization, and performance panels.
- `js/main.js`: API/fixture wiring, controls, loading, errors, and offline fallback.
- `fixtures/`: backend-generated demo reports; `tests/`: Node tests; `../server/server.js`: API/static server.

## Backend changes and limits

Additive backend fields and aliases belong in `js/adapter.js`; panels consume only its normalized model. Unknown keys are ignored and backend error payloads are preserved as `model.error`. The view is a connectivity visualization, not a hydraulic solver. Complete non-zero coordinates are used; otherwise a deterministic circular layout is used. SVG labels can overlap on very large graphs, uploaded `.inp` files are capped at 2 MB, and runs time out after 30 seconds.
