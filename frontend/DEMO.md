# Five-minute viva demo

1. Run `node server/server.js`, open `http://localhost:3000`, choose `sample.inp`, and click **Run simulation**. Explain the displayed node/link counts, disconnected junctions, disrupted demand, and legend as engine output.
2. Click pipe `P5` in the graph; it turns red and the status says it is selected. Click **Run simulation**. Explain the Impact counts and the Isolation valves-to-close/feasibility result.
3. Click **Reset**, then click valve `V1` to toggle stuck, click pipes `P3` and `P7`, and click **Run simulation**. Explain the extra-disruption count and the highlighted node(s).
4. Click the `P6` row in **Critical link ranking** to focus the graph. Click **Score** to reverse sort. Explain the engine-provided score and reason.
5. Compare all four rows in **Optimization**: baseline, optimized disruption, improvement %, runtime, and candidates. Click **Show proposed valves** for the before/after graph. Explain zero as zero, negative as regression, and any displayed exhaustive skip reason.

Stop the server and refresh to demonstrate the visible **offline demo** badge and fixture fallback. Upload malformed/partial JSON to demonstrate the readable error/empty states rather than a broken page.
