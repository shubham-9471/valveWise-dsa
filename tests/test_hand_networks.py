#!/usr/bin/env python3
import json, subprocess, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "backend" / ("valvewise.exe" if (ROOT / "backend" / "valvewise.exe").exists() else "valvewise")
CASES = [("ring6", "R12", 7, 7, 6.0), ("grid4x4", "G6", 17, 25, 2.0), ("tree_bridge", "T23", 6, 5, 14.0)]
for name, failure, nodes, links, demand in CASES:
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "out.json"
        p = subprocess.run([str(BIN), "--input", str(ROOT / "data" / f"{name}.inp"), "--valves", str(ROOT / "data" / f"{name}.valves"), "--fail", failure, "--output", str(out)], capture_output=True, text=True)
        assert p.returncode == 0, (name, p.stderr)
        d = json.loads(out.read_text()); assert len(d["graph"]["nodes"]) == nodes; assert len(d["graph"]["links"]) == links
        assert abs(d["isolation"]["total_disrupted_demand"] - demand) < 1e-9 and d["isolation"]["isolation_feasible"] is True
with tempfile.TemporaryDirectory() as td:
    bad = Path(td) / "bad.inp"; out = Path(td) / "bad.json"; bad.write_text("<!doctype html><html>404</html>")
    p = subprocess.run([str(BIN), "--input", str(bad), "--output", str(out)], capture_output=True, text=True)
    assert p.returncode != 0 and json.loads(out.read_text()).get("error")
print("A4 parser and hand-network tests passed")
