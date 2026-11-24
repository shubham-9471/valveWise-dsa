import { normalizeReport, readJsonFile } from './adapter.js';
import { NetworkView } from './graphView.js';

const fixtureSelect = document.querySelector('#fixture');
const resultUpload = document.querySelector('#upload');
const networkUpload = document.querySelector('#network-upload');
const status = document.querySelector('#status');
const errorBanner = document.querySelector('#error-banner');
const offlineBadge = document.querySelector('#offline-badge');
const budgetInput = document.querySelector('#budget');
const seedInput = document.querySelector('#seed');
const fixtureFiles = ['image3-budget-3.json', 'sample-multi-stuck.json', 'sample-no-failure.json', 'sample-single-failure.json'];
const labels = { 'image3-budget-3.json': 'Image 3 / budget 3', 'sample-multi-stuck.json': 'Sample / multi-failure + stuck valve', 'sample-no-failure.json': 'Sample / no failure', 'sample-single-failure.json': 'Sample / single failure' };
const networkFixture = { 'sample.inp': 'sample-no-failure.json', 'net_image3.inp': 'image3-budget-3.json' };
let currentModel = null; let currentName = ''; let uploadedNetworkText = '';
let controls = { failures: [], stuck: [] };

function showError(error) { errorBanner.hidden = false; errorBanner.textContent = typeof error === 'string' ? error : JSON.stringify(error?.error ?? error, null, 2); }
function clearError() { errorBanner.hidden = true; errorBanner.textContent = ''; }
function isValve(link) { return Boolean(link?.hasValve) || String(link?.type ?? '').toUpperCase() === 'VALVE'; }
function toggle(list, id) { return list.includes(id) ? list.filter(value => value !== id) : [...list, id]; }

function render(model, name, resetControls = true) {
  currentModel = model; currentName = name;
  if (resetControls) controls = { failures: [...model.failures], stuck: [...(model.stuckValves ?? [])] };
  document.querySelector('#report-name').textContent = name;
  document.querySelector('#summary').innerHTML = [['Nodes', model.nodes.length], ['Links', model.links.length], ['Disconnected junctions', model.disconnected.length], ['Disrupted demand', `${model.disruptedDemand} L/s`]].map(([label, value]) => `<article class="card"><strong>${value}</strong><span>${label}</span></article>`).join('');
  document.querySelector('#meta').innerHTML = `<dt>Sources</dt><dd>${model.sources.length}</dd><dt>Valves</dt><dd>${model.valves.length}</dd><dt>Failed links</dt><dd>${controls.failures.length}</dd><dt>Stuck valves</dt><dd>${controls.stuck.length}</dd><dt>Budget</dt><dd>${model.optimization.budget}</dd>`;
  budgetInput.value = model.optimization.budget;
  networkView.render({ ...model, failures: controls.failures, stuckValves: controls.stuck });
}

const networkView = new NetworkView(document.querySelector('#network'), (selection, item) => {
  if (selection.kind === 'link') {
    if (isValve(item)) controls.stuck = toggle(controls.stuck, item.id); else controls.failures = toggle(controls.failures, item.id);
    document.querySelector('#selection').textContent = `${isValve(item) ? 'Stuck valve' : 'Failed pipe'} ${item.id} ${controls.failures.includes(item.id) || controls.stuck.includes(item.id) ? 'selected' : 'cleared'}.`;
    render(currentModel, currentName, false);
  } else document.querySelector('#selection').textContent = `Selected ${selection.kind}: ${selection.id}`;
});

async function fixtureReport(name) { const response = await fetch(`fixtures/${encodeURIComponent(name)}`); if (!response.ok) throw new Error('Fixture unavailable'); return normalizeReport(await response.json()); }
async function selectedFixture() { return fixtureReport(networkFixture[fixtureSelect.value] || fixtureSelect.value); }
async function loadNetworkList() {
  fixtureFiles.forEach(file => fixtureSelect.add(new Option(labels[file], file)));
  try { const response = await fetch('/api/networks'); if (!response.ok) throw new Error(); const data = await response.json(); fixtureSelect.replaceChildren(...data.networks.map(name => new Option(name, name))); offlineBadge.hidden = true; } catch { offlineBadge.hidden = false; }
}
async function runSimulation() {
  clearError(); status.textContent = 'Running…'; document.querySelector('#run').disabled = true;
  const payload = { network: uploadedNetworkText || fixtureSelect.value, fail: controls.failures, stuck: controls.stuck, budget: Number(budgetInput.value) }; if (seedInput.value.trim()) payload.seed = Number(seedInput.value);
  try { const response = await fetch('/api/run', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) }); const data = await response.json(); if (!response.ok) throw data; uploadedNetworkText = ''; offlineBadge.hidden = true; render(normalizeReport(data), fixtureSelect.value); status.textContent = 'Engine result loaded'; } catch (error) { if (error instanceof TypeError || error?.message === 'Failed to fetch') { try { render(await selectedFixture(), `${fixtureSelect.value} — offline demo`); offlineBadge.hidden = false; status.textContent = 'Offline demo'; } catch (fallbackError) { showError(fallbackError); status.textContent = 'Run failed'; } } else { showError(error); status.textContent = 'Run failed'; } } finally { document.querySelector('#run').disabled = false; }
}

fixtureSelect.addEventListener('change', async () => { clearError(); uploadedNetworkText = ''; try { render(await selectedFixture(), fixtureSelect.value); status.textContent = 'Loaded'; } catch (error) { showError(error); } });
resultUpload.addEventListener('change', async () => { const file = resultUpload.files[0]; if (!file) return; try { render(await readJsonFile(file), file.name); status.textContent = 'Uploaded result'; } catch (error) { showError(`Could not read JSON: ${error.message}`); } });
networkUpload.addEventListener('change', async () => { const file = networkUpload.files[0]; if (!file) return; if (file.size > 2 * 1024 * 1024) { showError('Uploaded .inp file exceeds the 2 MB limit.'); return; } uploadedNetworkText = await file.text(); status.textContent = `${file.name} ready`; });
document.querySelector('#run').addEventListener('click', runSimulation);
document.querySelector('#reset').addEventListener('click', async () => { clearError(); uploadedNetworkText = ''; controls = { failures: [], stuck: [] }; budgetInput.value = 3; seedInput.value = ''; try { render(await selectedFixture(), fixtureSelect.value); status.textContent = 'Reset'; } catch (error) { showError(error); } });
document.querySelector('#fit-network').addEventListener('click', () => networkView.fit());
await loadNetworkList(); fixtureSelect.dispatchEvent(new Event('change'));
