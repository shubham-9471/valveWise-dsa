#!/usr/bin/env python3
"""Generate a connected rectangular EPANET grid with a reservoir feeder."""
import argparse, math
from pathlib import Path
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--nodes", type=int, choices=(1000, 10000), required=True); ap.add_argument("--out", type=Path, required=True); a = ap.parse_args()
    n = a.nodes - 1; cols = math.ceil(math.sqrt(n)); lines = ["[TITLE]", f"Synthetic {a.nodes}-node grid", "", "[JUNCTIONS]"]
    for i in range(n): lines.append(f"J{i+1} 0 1")
    lines += ["", "[RESERVOIRS]", "R1 100", "", "[PIPES]", "S R1 J1 1 100 100 OPEN"]; pid = 1
    for i in range(n):
        if i + 1 < n and (i + 1) % cols: lines.append(f"P{pid} J{i+1} J{i+2} 1 100 100 OPEN"); pid += 1
        if i + cols < n: lines.append(f"P{pid} J{i+1} J{i+cols+1} 1 100 100 OPEN"); pid += 1
    lines += ["", "[COORDINATES]"] + [f"J{i+1} {i % cols} {i // cols}" for i in range(n)] + ["R1 -1 0", "", "[END]", ""]
    a.out.write_text("\n".join(lines), encoding="utf-8"); print(a.out)
if __name__ == "__main__": main()
