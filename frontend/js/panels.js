const escapeHtml = value => String(value ?? '').replace(/[&<>"']/g, character => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[character]));
const number = value => Number.isFinite(Number(value)) ? String(Number(value)) : '—';
const list = (values, empty = 'None reported') => values?.length ? values.map(escapeHtml).join(', ') : empty;

export function renderImpact(container, model) {
  const scenario = model.explanations.scenarios[0] ?? {};
  container.innerHTML = `<h2>Impact</h2><div class="panel-grid"><div><strong>${model.disconnected.length}</strong><span>Disconnected junctions</span></div><div><strong>${number(model.disruptedDemand)} L/s</strong><span>Disrupted demand</span></div><div><strong>${model.isolation.valvesToClose.length}</strong><span>Valves to close</span></div><div><strong>${model.isolation.extraDisrupted.length}</strong><span>Extra disruption from stuck valves</span></div></div><dl class="panel-list"><dt>Disconnected junctions</dt><dd>${list(scenario.disconnectedJunctions?.length ? scenario.disconnectedJunctions : model.disconnected)}</dd><dt>Valves to close</dt><dd>${list(model.isolation.valvesToClose)}</dd><dt>Stuck boundary</dt><dd>${list((scenario.stuckBoundary ?? []).map(item => item.link))}</dd></dl>`;
}

export function renderIsolation(container, model) {
  const scenarios = model.explanations.scenarios ?? [];
  const feasible = model.isolation.isolationFeasible;
  const status = feasible === false ? 'Isolation is not feasible; the backend applied worst-case network demand.' : feasible === true ? 'Isolation is feasible for the reported failure segment.' : 'Feasibility was not reported by the backend.';
  const details = scenarios.map(scenario => `<article class="explanation"><strong>Scenario ${scenario.scenario || '—'}: ${scenario.isolationFeasible === false ? 'not feasible' : 'feasible'}</strong><p>Failure links: ${list(scenario.failedLinks)}</p><p>Valves: ${list(scenario.valvesToClose.map(item => `${item.link} — ${item.reason}`))}</p><p>Stuck boundary: ${list(scenario.stuckBoundary.map(item => `${item.link} — ${item.reason}`))}</p><p>Alternative routes: ${list(scenario.alternativeRoutes.map(route => `${route.source}: ${route.viaLinks.join(' → ')}`))}</p></article>`).join('');
  container.innerHTML = `<h2>Isolation</h2><p class="feasibility ${feasible === true ? 'positive' : ''}">${escapeHtml(status)}</p>${details || '<p>No scenario explanation was supplied.</p>'}`;
}

export function renderRanking(container, model, onFocus) {
  let rows = [...model.rankedLinks]; let sortKey = 'score'; let direction = -1;
  const draw = () => { rows.sort((a, b) => { const left = a[sortKey] ?? ''; const right = b[sortKey] ?? ''; return (typeof left === 'number' ? left - right : String(left).localeCompare(String(right))) * direction; }); container.innerHTML = `<h2>Critical link ranking</h2><table class="data-table"><thead><tr><th data-sort="linkId">Link</th><th data-sort="score">Score</th><th>Reason</th></tr></thead><tbody>${rows.map(row => `<tr data-link-id="${escapeHtml(row.linkId)}"><td>${escapeHtml(row.linkId)}</td><td>${number(row.score)}</td><td>${escapeHtml(row.reason)}</td></tr>`).join('')}</tbody></table>`; container.querySelectorAll('[data-sort]').forEach(header => header.addEventListener('click', () => { const key = header.dataset.sort; direction = sortKey === key ? direction * -1 : key === 'score' ? -1 : 1; sortKey = key; draw(); })); container.querySelectorAll('tbody tr').forEach(row => row.addEventListener('click', () => onFocus(row.dataset.linkId))); };
  draw();
}

export function renderOptimization(container, model) {
  const methods = ['greedy', 'random', 'topology', 'exhaustive'];
  const results = methods.map(key => ({ key, result: model.optimization[key] ?? {} }));
  const values = results.map(item => Number(item.result.totalDisruption) || 0); const max = Math.max(1, ...values.map(value => Math.abs(value))); const width = 440;
  const barRows = results.map((item, index) => { const value = Number(item.result.totalDisruption) || 0; const barWidth = Math.abs(value) / max * width; const x = value < 0 ? 220 - barWidth : 220; return `<text x="0" y="${24 + index * 32}" class="chart-label">${escapeHtml(item.result.algorithm || item.key)}</text><rect x="${x}" y="${10 + index * 32}" width="${barWidth}" height="18" class="chart-bar ${item.key}"></rect><text x="${Math.min(680, 235 + barWidth)}" y="${24 + index * 32}" class="chart-value">${number(value)}</text>`; }).join('');
  const rows = results.map(item => { const result = item.result; if (result.skipped) return `<tr><td>${escapeHtml(result.algorithm || item.key)}</td><td colspan="7">Skipped: ${escapeHtml(result.skippedReason || 'No reason supplied')}</td></tr>`; const placements = result.placementExplanations.map(entry => `${entry.valve}: ${entry.why}`).join('<br>'); return `<tr><td>${escapeHtml(result.algorithm || item.key)}</td><td>${number(result.baselineDisruption)}</td><td>${number(result.totalDisruption)}</td><td>${number(result.improvementPct)}%</td><td>${number(result.meanDisruption)}</td><td>${number(result.bestDisruption)}</td><td>${number(result.runtimeMs)} ms</td><td>${number(result.candidatesEvaluated)}<br><small>${escapeHtml(placements)}</small></td></tr>`; }).join('');
  container.innerHTML = `<h2>Optimization</h2><p>Seed ${number(model.optimization.seed)} · ${number(model.optimization.scenarioCount)} scenario(s)</p><table class="data-table optimization-table"><thead><tr><th>Method</th><th>Baseline</th><th>Optimized</th><th>Improvement %</th><th>Mean</th><th>Best</th><th>Runtime</th><th>Candidates / placement reasons</th></tr></thead><tbody>${rows}</tbody></table><div class="chart-wrap"><svg class="optimization-chart" viewBox="0 0 700 145" role="img" aria-label="Optimization disruption comparison"><line x1="220" y1="0" x2="220" y2="140" class="chart-axis"></line>${barRows}</svg></div>`;
}

export function renderPerformance(container, model) {
  const performance = model.performance ?? {};
  const memory = performance.memoryKb == null || performance.memoryKb < 0 ? '—' : `${number(performance.memoryKb)} KB`;
  container.innerHTML = `<h2>Performance</h2><div class="panel-grid"><div><strong>${number(performance.bfsMs)} ms</strong><span>BFS time</span></div><div><strong>${number(performance.optimizationRuntimeMs)} ms</strong><span>Optimization runtime</span></div><div><strong>${number(performance.candidates)}</strong><span>Candidates evaluated</span></div><div><strong>${memory}</strong><span>Peak memory</span></div></div>`;
}
