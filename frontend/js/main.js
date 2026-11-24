import { normalizeReport, readJsonFile } from './adapter.js';

const fixtureSelect = document.querySelector('#fixture');
const upload = document.querySelector('#upload');
const status = document.querySelector('#status');
const fixtureFiles = ['image3-budget-3.json', 'sample-multi-stuck.json', 'sample-no-failure.json', 'sample-single-failure.json'];
const labels = { 'image3-budget-3.json': 'Image 3 / budget 3', 'sample-multi-stuck.json': 'Sample / multi-failure + stuck valve', 'sample-no-failure.json': 'Sample / no failure', 'sample-single-failure.json': 'Sample / single failure' };

function render(model, name) {
  document.querySelector('#report-name').textContent = name;
  document.querySelector('#summary').innerHTML = [['Nodes', model.nodes.length], ['Links', model.links.length], ['Disconnected junctions', model.disconnected.length], ['Disrupted demand', `${model.disruptedDemand} L/s`]].map(([label, value]) => `<article class="card"><strong>${value}</strong><span>${label}</span></article>`).join('');
  document.querySelector('#meta').innerHTML = `<dt>Sources</dt><dd>${model.sources.length}</dd><dt>Valves</dt><dd>${model.valves.length}</dd><dt>Failed links</dt><dd>${model.failures.length}</dd><dt>Isolation valves</dt><dd>${model.isolation.valvesToClose.length}</dd><dt>Budget</dt><dd>${model.optimization.budget}</dd>`;
}
async function loadFixture(name) { const response = await fetch(`fixtures/${encodeURIComponent(name)}`); if (!response.ok) throw new Error('Fixture unavailable'); return normalizeReport(await response.json()); }
fixtureFiles.forEach(file => fixtureSelect.add(new Option(labels[file], file)));
fixtureSelect.addEventListener('change', async () => { try { render(await loadFixture(fixtureSelect.value), labels[fixtureSelect.value]); status.textContent = 'Loaded'; } catch (error) { status.textContent = error.message; } });
upload.addEventListener('change', async () => { const file = upload.files[0]; if (!file) return; try { render(await readJsonFile(file), file.name); status.textContent = 'Uploaded'; } catch (error) { status.textContent = `Could not read JSON: ${error.message}`; } });
fixtureSelect.dispatchEvent(new Event('change'));
