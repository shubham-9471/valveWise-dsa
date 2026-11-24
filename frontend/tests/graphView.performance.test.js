import test from 'node:test';
import assert from 'node:assert/strict';
import { layoutNodes } from '../js/graphView.js';

test('lays out a 2000-node fallback graph without quadratic work', () => {
  const nodes = Array.from({ length: 2000 }, (_, index) => ({ id: `J${index}`, x: 0, y: 0 }));
  const started = performance.now(); const positions = layoutNodes(nodes); const elapsed = performance.now() - started;
  assert.equal(positions.size, 2000); assert.ok(elapsed < 1000, `2000-node layout took ${elapsed.toFixed(1)} ms`); console.log(`2000-node layout: ${elapsed.toFixed(1)} ms`);
});
