#!/usr/bin/env python3
"""Generate deterministic external valves for pipes incident to degree>=3 nodes."""
import argparse, random, re
from pathlib import Path
def parse(path):
    sec = None; pipes = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split(";", 1)[0].strip()
        if not line: continue
        m = re.match(r"\[([^]]+)\]", line)
        if m: sec = m.group(1).upper(); continue
        f = line.split()
        if sec == "PIPES" and len(f) >= 3: pipes.append((f[0], f[1], f[2]))
    degree = {}
    for _, a, b in pipes: degree[a] = degree.get(a, 0) + 1; degree[b] = degree.get(b, 0) + 1
    return pipes, degree
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--inp", required=True, type=Path); ap.add_argument("--seed", required=True, type=int); ap.add_argument("--density", required=True, type=float); ap.add_argument("--out", type=Path)
    a = ap.parse_args()
    if not 0 <= a.density <= 1: ap.error("--density must be in [0,1]")
    pipes, degree = parse(a.inp); rng = random.Random(a.seed)
    chosen = [pid for pid, x, y in pipes if (degree[x] >= 3 or degree[y] >= 3) and rng.random() < a.density]
    out = a.out or a.inp.with_suffix(".valves"); out.write_text("; seed={} density={}\n{}\n".format(a.seed, a.density, "\n".join(chosen)), encoding="utf-8"); print(out)
if __name__ == "__main__": main()
