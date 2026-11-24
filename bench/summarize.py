#!/usr/bin/env python3
import argparse, csv, json, subprocess, tempfile
from pathlib import Path
NETWORKS = ["sample", "net_image3", "ring6", "grid4x4", "tree_bridge", "Net1", "Net3"]
BUDGETS = [1, 2, 3, 5]
ALGORITHMS = ["greedy", "random", "topology", "exhaustive"]
def run(repo):
    binary = repo / "backend" / ("valvewise.exe" if (repo / "backend" / "valvewise.exe").exists() else "valvewise"); rows = []
    with tempfile.TemporaryDirectory() as td:
        for network in NETWORKS:
            inp = repo / "data" / f"{network}.inp"; valves = repo / "data" / f"{network}.valves"
            for budget in BUDGETS:
                for rep in range(5):
                    seed = 1000 + rep; out = Path(td) / f"{network}_{budget}_{rep}.json"
                    cmd = [str(binary), "--input", str(inp), "--budget", str(budget), "--seed", str(seed), "--scenarios", "all", "--output", str(out)]
                    if valves.exists(): cmd += ["--valves", str(valves)]
                    subprocess.run(cmd, check=True, capture_output=True, text=True); opt = json.loads(out.read_text())["optimization"]
                    for algorithm in ALGORITHMS:
                        if algorithm == "random" or rep == 0:
                            item = opt[algorithm]; rows.append({"network": network, "budget": budget, "algorithm": algorithm, "repetition": rep, "seed": seed, "baseline": item["baseline_disruption"], "final_disruption": item["total_disruption"], "improvement_pct": item["improvement_pct"], "runtime_ms": item["runtime_ms"], "candidates": item["candidates_evaluated"]})
    path = repo / "bench" / "results.csv"
    with path.open("w", newline="") as fp: writer = csv.DictWriter(fp, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    print(path)
def summarize(path):
    with path.open(newline="") as fp: rows = list(csv.DictReader(fp))
    for algorithm in ALGORITHMS:
        values = [float(r["improvement_pct"]) for r in rows if r["algorithm"] == algorithm]; print(f"{algorithm}: mean improvement {sum(values)/len(values):.2f}%")
    gaps = []
    for r in rows:
        if r["algorithm"] != "greedy": continue
        for g in rows:
            if g["algorithm"] == "exhaustive" and (g["network"], g["budget"]) == (r["network"], r["budget"]): gaps.append(float(r["final_disruption"]) - float(g["final_disruption"]))
    print(f"greedy minus exhaustive final-disruption gap: {sum(gaps)/len(gaps):.3f} L/s")
if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("--run", action="store_true"); ap.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1]); a = ap.parse_args()
    if a.run: run(a.repo)
    else: summarize(a.repo / "bench" / "results.csv")
