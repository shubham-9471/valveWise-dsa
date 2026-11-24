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

test('normalizes additive impact, explanations, optimization extras, and performance fields', () => {
  const model = normalizeReport({ isolation: { isolation_feasible: false, extra_disrupted: ['J2'] }, explanations: { scenarios: [{ scenario: 1, valves_to_close: [{ link: 'V1', reason: 'bounds segment 1' }], alternative_routes: [{ source: 'R1', via_links: ['P1'] }], isolation_feasible: false }] }, optimization: { seed: 7, scenario_count: 1, greedy: { algorithm: 'Greedy', mean_disruption: 3, best_disruption: 2, placement_explanations: [{ valve: 'P2', before: 4, after: 2, why: 'improved' }] } }, performance: { bfs_ms: 1.25, memory_kb: 2048 }, ranked_links: [{ link_id: 'P1', score: -2, reason: 'reason' }] });
  assert.equal(model.isolation.isolationFeasible, false); assert.deepEqual(model.isolation.extraDisrupted, ['J2']); assert.equal(model.explanations.scenarios[0].valvesToClose[0].reason, 'bounds segment 1');
  assert.equal(model.explanations.scenarios[0].alternativeRoutes[0].viaLinks[0], 'P1'); assert.equal(model.optimization.seed, 7); assert.equal(model.optimization.greedy.bestDisruption, 2); assert.equal(model.optimization.greedy.placementExplanations[0].why, 'improved');
  assert.equal(model.performance.bfsMs, 1.25); assert.equal(model.performance.memoryKb, 2048); assert.equal(model.rankedLinks[0].score, -2);
});

test('handles empty arrays, malformed shapes, and backend error JSON safely', () => {
  const model = normalizeReport({ graph: { nodes: [], links: [] }, reachability: { disconnected_nodes: [] }, isolation: { failed_links: [], valves_to_close: [] }, ranked_links: [], error: { code: 'BACKEND_FAILED', message: 'engine failed' }, future: { ignored: true } });
  assert.deepEqual(model.nodes, []); assert.deepEqual(model.links, []); assert.deepEqual(model.rankedLinks, []); assert.equal(model.error.code, 'BACKEND_FAILED');
  assert.deepEqual(normalizeReport(null).nodes, []); assert.deepEqual(normalizeReport('partial').links, []);
  assert.equal(normalizeReport({ error: 'invalid or empty input network' }).error.message, 'invalid or empty input network');
});

test('normalizes hydraulic states without inventing missing values', () => {
  const model = normalizeReport({ hydraulics: { available: true, engine: 'demo', units: { pressure: 'm', flow: 'L/s' }, min_pressure: 20, baseline: { nodes: [{ id: 'J1', pressure: 15, served: false }], pressure_deficient: ['J1'], unserved_demand: 4 }, after_failure: null, future_field: true } });
  assert.equal(model.hydraulics.available, true); assert.equal(model.hydraulics.minPressure, 20); assert.equal(model.hydraulics.states.baseline.nodes[0].pressure, 15); assert.deepEqual(model.hydraulics.states.baseline.pressureDeficient, ['J1']); assert.equal(model.hydraulics.states.after_failure.nodes.length, 0); assert.equal(model.hydraulics.states.baseline.unservedDemand, 4);
  const unavailable = normalizeReport({ hydraulics: { available: false, baseline: { nodes: [{ id: 'J1', pressure: 99 }] } } }); assert.equal(unavailable.hydraulics.available, false);
});
