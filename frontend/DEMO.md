# Five-minute viva demo

1. Run `node server/server.js`, open `http://localhost:3000`, choose `sample.inp`, and click **Run simulation**. The generated `sample-real.json` run has 11 nodes, 14 links, and 174 L/s disrupted demand for failure `P1`; explain these as engine output.
2. Click pipe `P5` in the graph; it turns red and the status says it is selected. Click **Run simulation**. Explain the Impact counts and the Isolation valves-to-close/feasibility result, including the backend's `isolation_feasible` flag.
3. Click **Reset**, then click valve `V1` to toggle stuck, click pipes `P3` and `P7`, and click **Run simulation**. Explain the extra-disruption count and the highlighted node(s).
4. Click the `P6` row in **Critical link ranking** to focus the graph. Click **Score** to reverse sort. Explain the engine-provided score and reason.
5. Compare all four rows in **Optimization**: baseline, optimized disruption, improvement %, runtime, and candidates. Click **Show proposed valves** for the before/after graph. Explain zero as zero, negative as regression, and any displayed exhaustive skip reason.

Stop the server and refresh to demonstrate the visible **offline demo** badge and fixture fallback. Upload malformed/partial JSON to demonstrate the readable error/empty states rather than a broken page.

## Generated fixture reference

The checked-in fixtures were regenerated from the backend CLI with budget 3, seed 17, and a real failure (and an external valves file where available): Image 3 is 5 nodes / 5 links / 35 L/s, ring6 is 7 / 7 / 6 L/s, grid4x4 is 17 / 25 / 16 L/s, tree_bridge is 6 / 5 / 15 L/s, Net1 is 11 / 13 / 950 L/s, and Net3 is 11 / 12 / 270 L/s. These are connectivity-simulator outputs, not hydraulic predictions.
