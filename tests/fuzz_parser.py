#!/usr/bin/env python3
"""Twenty deterministic truncation/mangling parser smoke cases."""
import json, random, subprocess, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "backend" / ("valvewise.exe" if (ROOT / "backend" / "valvewise.exe").exists() else "valvewise")
source = (ROOT / "data" / "sample.inp").read_bytes(); rng = random.Random(0xA4F00D)
mutations = []
for i in range(20):
    data = bytearray(source[:rng.randrange(0, len(source) + 1)])
    if data: data[rng.randrange(len(data))] = rng.randrange(32, 127)
    if i % 4 == 0: data = bytearray(b"<!doctype html><html>404</html>")
    elif i % 4 == 1: data = bytearray(b"[JUNCTIONS]\nnot a record\n[PIPES]\n")
    mutations.append(bytes(data))
with tempfile.TemporaryDirectory() as td:
    for i, data in enumerate(mutations):
        inp, out = Path(td) / f"fuzz{i}.inp", Path(td) / f"fuzz{i}.json"; inp.write_bytes(data)
        p = subprocess.run([str(BIN), "--input", str(inp), "--output", str(out)], capture_output=True)
        assert out.exists(), (i, p.returncode, p.stderr.decode(errors="replace"))
        json.loads(out.read_text())
print("parser fuzz: 20 truncated/mangled inputs completed without crash")
