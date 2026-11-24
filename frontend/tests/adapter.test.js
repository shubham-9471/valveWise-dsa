import test from 'node:test';
import assert from 'node:assert/strict';
import { normalizeReport } from '../js/adapter.js';

test('normalizes documented report and derives sources, valves, and counts', () => {
  const model = normalizeReport({ graph: { nodes: [{ id: 'R1', type: 'RESERVOIR', is_source: true }, { id: 'J1', type: 'JUNCTION', base_demand: 4 }], links: [{ id: 'V1', from_id: 'R1', to_id: 'J1', link_type: 'VALVE', has_valve: true }] }, reachability: { disconnected_nodes: [{ id: 'J1' }], disconnected_demand: 4 }, isolation: { failed_links: ['V1'], valves_to_close: ['V1'], total_disrupted_demand: 7 }, optimization: { budget: 3 } });
  assert.equal(model.nodes.length, 2); assert.equal(model.links.length, 1); assert.deepEqual(model.sources, ['R1']);
  assert.deepEqual(model.failures, ['V1']); assert.deepEqual(model.disconnected, ['J1']); assert.equal(model.disruptedDemand, 7);
  assert.equal(model.optimization.budget, 3); assert.equal(model.valves.length, 1);
});

test('tolerates missing sections, nulls, and extra fields', () => {
  const model = normalizeReport({ future_field: { ignored: true }, graph: { nodes: null, links: null }, reachability: null, isolation: null });
  assert.deepEqual(model.nodes, []); assert.deepEqual(model.links, []); assert.equal(model.disruptedDemand, 0);
  assert.deepEqual(model.isolation.valvesToClose, []);
});
