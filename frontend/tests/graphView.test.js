import test from 'node:test';
import assert from 'node:assert/strict';
import { getLinkVisual, getNodeVisual, nodeTypeKey } from '../js/graphView.js';

test('maps node types to distinct shapes and source styling', () => {
  assert.equal(nodeTypeKey({ type: 'JUNCTION' }), 'junction');
  assert.equal(getNodeVisual({ type: 'RESERVOIR' }).shape, 'rect');
  assert.equal(getNodeVisual({ type: 'TANK' }).shape, 'diamond');
  assert.equal(getNodeVisual({ type: 'PUMP_END' }).shape, 'hexagon');
});

test('prioritizes link state colors and preserves valve markers', () => {
  assert.equal(getLinkVisual({ type: 'PIPE' }, { failed: true }).stroke, '#ed5c5c');
  assert.equal(getLinkVisual({ type: 'VALVE' }, { closed: true }).marker, true);
  assert.equal(getLinkVisual({ type: 'PIPE' }, { proposed: true }).stroke, '#36d399');
  assert.equal(getLinkVisual({ hasValve: true }).marker, true);
});
