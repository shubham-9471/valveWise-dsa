# Hand-checked A4 fixtures

The source node has zero demand. Each listed failure is one scenario and the
companion valve file is loaded with `--valves`.

| network | nodes | links | failure | expected disrupted demand | expected feasible |
|---|---:|---:|---|---:|---|
| ring6 | 7 | 7 | R12 | 6 | true |
| grid4x4 | 17 | 25 | G6 | 2 | true |
| tree_bridge | 6 | 5 | T23 | 14 | true |
