#!/usr/bin/env python3
import json, pathlib, subprocess, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
BIN = ROOT / 'backend' / ('valvewise.exe' if (ROOT / 'backend' / 'valvewise.exe').exists() else 'valvewise')
def run(inp, *args):
    with tempfile.NamedTemporaryFile(suffix='.json', delete=False) as f: out = pathlib.Path(f.name)
    try:
        p = subprocess.run([str(BIN), '--input', str(ROOT/'data'/inp), '--output', str(out), *args], capture_output=True, text=True, timeout=30)
        assert p.returncode == 0, p.stderr
        return json.loads(out.read_text())
    finally: out.unlink(missing_ok=True)
def node(state, ident): return next(x for x in state['nodes'] if x['id'] == ident)
net1 = run('Net1.inp', '--fail', '11p', '--budget', '1')
assert net1['hydraulics']['available'] is True
assert {'baseline','after_failure','after_isolation','after_optimization'} <= set(net1['hydraulics'])
line = run('hydraulic_line3.inp', '--fail', 'P2', '--budget', '0')
b, f = node(line['hydraulics']['baseline'], 'J2'), node(line['hydraulics']['after_failure'], 'J2')
assert f['pressure'] < b['pressure'] and f['demand'] < b['demand'], (b, f)
off = run('hydraulic_line3.inp', '--fail', 'P2', '--hydraulics', 'off')
assert off['hydraulics']['available'] is False and off['graph'] == line['graph'] and off['isolation'] == line['isolation']
print('H1 hydraulics tests passed')
