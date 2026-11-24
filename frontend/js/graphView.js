const SVG_NS = 'http://www.w3.org/2000/svg';
const finite = value => Number.isFinite(Number(value));

export function nodeTypeKey(node = {}) {
  const type = String(node.type ?? '').toUpperCase().replace(/[- ]/g, '_');
  if (type.includes('RESERVOIR') || node.isSource) return 'reservoir';
  if (type.includes('TANK')) return 'tank';
  if (type.includes('PUMP')) return 'pump-end';
  return 'junction';
}

export function pressureColor(value, min, max) {
  if (!finite(value)) return '#58736d';
  const ratio = Math.max(0, Math.min(1, (Number(value) - Number(min)) / Math.max(Number(max) - Number(min), 1e-9)));
  return `hsl(${Math.round(ratio * 120)} 70% 45%)`;
}

export function getNodeVisual(node = {}, state = {}) {
  const type = nodeTypeKey(node);
  const fill = state.pressureLayer ? pressureColor(state.pressure, state.pressureMin, state.pressureMax) : type === 'reservoir' ? '#43b7a0' : type === 'tank' ? '#d19a4a' : type === 'pump-end' ? '#b477dc' : '#5b8dd9';
  const base = { type, shape: type === 'reservoir' ? 'rect' : type === 'tank' ? 'diamond' : type === 'pump-end' ? 'hexagon' : 'circle', fill, stroke: '#dce9e5', opacity: 1 };
  if (state.disconnected) return { ...base, fill: '#697570', stroke: state.pressureDeficient ? '#ff5c5c' : '#e4efeb', strokeWidth: state.pressureDeficient ? 3 : 1.5, strokeDasharray: '3 2', opacity: 0.55 };
  if (state.pressureDeficient) return { ...base, stroke: '#ff5c5c', strokeWidth: 3 };
  if (state.affected) return { ...base, stroke: '#ff9f43', strokeWidth: 3 };
  if (state.selected) return { ...base, stroke: '#fff3a8', strokeWidth: 3 };
  return base;
}

export function getLinkVisual(link = {}, state = {}) {
  const type = String(link.type ?? '').toUpperCase(); const valve = Boolean(link.hasValve) || type === 'VALVE';
  let visual = { stroke: valve ? '#8ab8ad' : '#58736d', width: valve ? 2.5 : 1.5, opacity: 0.9, dash: '', marker: valve };
  if (state.failed) visual = { stroke: '#ed5c5c', width: 4, opacity: 1, dash: '', marker: valve };
  else if (state.stuck) visual = { stroke: '#d58bff', width: 3, opacity: 1, dash: '7 4', marker: true };
  else if (state.closed) visual = { stroke: '#ffd166', width: 3, opacity: 1, dash: '', marker: true };
  else if (state.proposed) visual = { stroke: '#36d399', width: 3, opacity: 1, dash: '3 3', marker: true };
  else if (state.affected) visual = { stroke: '#ff9f43', width: 3, opacity: 1, dash: '2 4', marker: valve };
  if (state.pressureLayer && finite(state.flow)) visual.width = Math.max(1.5, Math.min(8, 1.5 + Math.abs(Number(state.flow)) / Math.max(Number(state.flowMax), 1e-9) * 6));
  return visual;
}

function makeSvg(tag, attrs = {}) { const element = document.createElementNS(SVG_NS, tag); Object.entries(attrs).forEach(([key, value]) => element.setAttribute(key, String(value))); return element; }

export function layoutNodes(nodes) {
  const hasCoordinates = nodes.length > 0 && nodes.every(node => finite(node.x) && finite(node.y)) && nodes.some(node => Number(node.x) !== 0 || Number(node.y) !== 0);
  const positions = new Map();
  if (hasCoordinates) {
    const xs = nodes.map(node => Number(node.x)); const ys = nodes.map(node => Number(node.y)); const minX = Math.min(...xs); const maxX = Math.max(...xs); const minY = Math.min(...ys); const maxY = Math.max(...ys); const scale = Math.max(maxX - minX, maxY - minY, 1);
    nodes.forEach(node => positions.set(node.id, { x: 80 + ((Number(node.x) - minX) / scale) * 840, y: 70 + ((maxY - Number(node.y)) / scale) * 560 })); return positions;
  }
  const radius = Math.max(110, Math.min(260, nodes.length * 18));
  nodes.forEach((node, index) => { const angle = (index / Math.max(nodes.length, 1)) * Math.PI * 2 - Math.PI / 2; positions.set(node.id, { x: 500 + Math.cos(angle) * radius, y: 350 + Math.sin(angle) * radius }); }); return positions;
}

export class NetworkView {
  constructor(container, onSelect = () => {}) { this.container = container; this.onSelect = onSelect; this.selection = null; this.zoom = 1; this.pan = { x: 0, y: 0 }; }
  render(model) {
    const started = performance.now(); this.model = model; this.positions = layoutNodes(model.nodes); this.items = new Map(); this.zoom = 1; this.pan = { x: 0, y: 0 }; this.container.replaceChildren();
    const wrapper = document.createElement('div'); wrapper.className = 'network-canvas'; const root = makeSvg('svg', { viewBox: '0 0 1000 700', role: 'img', 'aria-label': 'ValveWise network graph' }); const graph = makeSvg('g'); const linksLayer = makeSvg('g', { class: 'network-links' }); const nodesLayer = makeSvg('g', { class: 'network-nodes' }); graph.append(linksLayer, nodesLayer); root.append(graph); wrapper.append(root); const tooltip = document.createElement('div'); tooltip.className = 'network-tooltip'; tooltip.hidden = true; wrapper.append(tooltip); this.container.append(wrapper); this.svgRoot = root; this.graph = graph; this.tooltip = tooltip;
    const failed = new Set(model.failures); const closed = new Set(model.isolation?.valvesToClose ?? []); const stuck = new Set(model.stuckValves ?? []); const proposed = new Set(model.proposedValves ?? []); const affected = new Set(model.isolation?.affectedSections ?? []); const disconnected = new Set(model.disconnected ?? []); const extra = new Set(model.isolation?.extraDisrupted ?? []);
    const hydraulic = model.layer === 'pressure' && model.hydraulics?.available ? model.hydraulics.states?.[model.hydraulicState] : null; const pressureById = new Map((hydraulic?.nodes ?? []).map(node => [node.id, node.pressure])); const deficient = new Set(hydraulic?.pressureDeficient ?? []); const flowById = new Map((hydraulic?.links ?? []).map(link => [link.id, link.flow])); const flowMax = Math.max(0, ...[...flowById.values()].map(value => Math.abs(Number(value) || 0))); const pressureValues = [...pressureById.values()].filter(finite); const pressureMax = Math.max(...pressureValues, model.hydraulics?.minPressure ?? 1);
    model.links.forEach(link => { const a = this.positions.get(link.from); const b = this.positions.get(link.to); if (!a || !b) return; const visual = getLinkVisual(link, { failed: failed.has(link.id), closed: closed.has(link.id), stuck: stuck.has(link.id) || /STUCK/i.test(link.status), proposed: proposed.has(link.id), affected: affected.has(link.id), pressureLayer: Boolean(hydraulic), flow: flowById.get(link.id), flowMax }); const line = makeSvg('line', { x1: a.x, y1: a.y, x2: b.x, y2: b.y, stroke: visual.stroke, 'stroke-width': visual.width, 'stroke-dasharray': visual.dash, opacity: visual.opacity, 'vector-effect': 'non-scaling-stroke' }); line.dataset.id = link.id; this.bindItem(line, 'link', link); linksLayer.append(line); if (visual.marker) { const mx = (a.x + b.x) / 2; const my = (a.y + b.y) / 2; const marker = makeSvg('polygon', { points: `${mx},${my - 6} ${mx + 6},${my} ${mx},${my + 6} ${mx - 6},${my}`, fill: visual.stroke, class: 'valve-marker' }); marker.dataset.id = link.id; this.bindItem(marker, 'link', link); linksLayer.append(marker); } });
    model.nodes.forEach(node => { const point = this.positions.get(node.id); if (!point) return; const visual = getNodeVisual(node, { disconnected: disconnected.has(node.id), affected: extra.has(node.id), selected: this.selection?.id === node.id, pressureLayer: Boolean(hydraulic), pressure: pressureById.get(node.id), pressureMin: model.hydraulics?.minPressure ?? 0, pressureMax, pressureDeficient: deficient.has(node.id) }); const group = makeSvg('g', { class: 'network-node', transform: `translate(${point.x} ${point.y})`, opacity: visual.opacity }); group.dataset.id = node.id; let shape; if (visual.shape === 'rect') shape = makeSvg('rect', { x: -9, y: -9, width: 18, height: 18, rx: 3 }); else if (visual.shape === 'diamond') shape = makeSvg('polygon', { points: '0,-12 12,0 0,12 -12,0' }); else if (visual.shape === 'hexagon') shape = makeSvg('polygon', { points: '-10,-6 -5,-11 5,-11 10,-6 5,11 -5,11' }); else shape = makeSvg('circle', { r: 8 }); shape.setAttribute('fill', visual.fill); shape.setAttribute('stroke', visual.stroke); shape.setAttribute('stroke-width', visual.strokeWidth ?? 1.5); if (visual.strokeDasharray) shape.setAttribute('stroke-dasharray', visual.strokeDasharray); shape.setAttribute('vector-effect', 'non-scaling-stroke'); group.append(shape); const label = makeSvg('text', { x: 12, y: 4 }); label.textContent = node.id; group.append(label); this.bindItem(group, 'node', node); nodesLayer.append(group); });
    this.bindInteractions(root); this.bindViewport(root); this.fit(); this.lastRenderMs = performance.now() - started; this.container.dataset.renderMs = this.lastRenderMs.toFixed(1);
  }
  bindItem(element, kind, item) { element.dataset.kind = kind; element.setAttribute('tabindex', '0'); element.setAttribute('aria-label', `${kind} ${item.id}`); this.items.set(`${kind}:${item.id}`, item); }
  bindInteractions(root) { root.addEventListener('pointerover', event => { const element = event.target.closest?.('[data-id]'); if (!element) return; const item = this.items.get(`${element.dataset.kind}:${element.dataset.id}`); if (item) this.showTooltip(event, element.dataset.kind, item); }); root.addEventListener('pointerout', event => { if (!event.relatedTarget || !event.target.closest?.('[data-id]')?.contains(event.relatedTarget)) this.hideTooltip(); }); const activate = event => { const element = event.target.closest?.('[data-id]'); if (!element) return; const item = this.items.get(`${element.dataset.kind}:${element.dataset.id}`); if (item) { event.preventDefault(); this.selection = { kind: element.dataset.kind, id: item.id }; this.onSelect(this.selection, item); } }; root.addEventListener('click', activate); root.addEventListener('keydown', event => { if (event.key === 'Enter' || event.key === ' ') activate(event); }); }
  showTooltip(event, kind, item) { this.tooltip.textContent = kind === 'node' ? `${item.id}\nDemand: ${item.demand ?? 0}` : `${item.id}\nLength: ${item.length ?? 0}\nStatus: ${item.status ?? 'UNKNOWN'}`; this.tooltip.hidden = false; const rect = this.container.getBoundingClientRect(); this.tooltip.style.left = `${event.clientX - rect.left + 12}px`; this.tooltip.style.top = `${event.clientY - rect.top + 12}px`; }
  hideTooltip() { if (this.tooltip) this.tooltip.hidden = true; }
  fit() { if (!this.svgRoot) return; this.zoom = 1; this.pan = { x: 0, y: 0 }; this.graph.setAttribute('transform', 'translate(0 0) scale(1)'); }
  focus(id) { const link = this.model?.links.find(item => item.id === id); const target = this.positions?.get(id) || (link && this.positions.get(link.from)); if (!target || !this.graph) return; this.selection = { kind: 'link', id }; this.zoom = 2.2; this.pan = { x: 500 - target.x * this.zoom, y: 350 - target.y * this.zoom }; this.graph.setAttribute('transform', `translate(${this.pan.x} ${this.pan.y}) scale(${this.zoom})`); this.svgRoot.querySelectorAll('[data-id]').forEach(element => element.classList.toggle('network-focus', element.dataset.id === id)); }
  bindViewport(root) { root.addEventListener('wheel', event => { event.preventDefault(); this.zoom = Math.max(0.3, Math.min(5, this.zoom * (event.deltaY < 0 ? 1.12 : 0.89))); this.graph.setAttribute('transform', `translate(${this.pan.x} ${this.pan.y}) scale(${this.zoom})`); }, { passive: false }); let start = null; root.addEventListener('pointerdown', event => { if (event.target !== root) return; start = { x: event.clientX, y: event.clientY, pan: { ...this.pan } }; root.setPointerCapture(event.pointerId); }); root.addEventListener('pointermove', event => { if (!start) return; this.pan = { x: start.pan.x + (event.clientX - start.x) / 1.2, y: start.pan.y + (event.clientY - start.y) / 1.2 }; this.graph.setAttribute('transform', `translate(${this.pan.x} ${this.pan.y}) scale(${this.zoom})`); }); root.addEventListener('pointerup', () => { start = null; }); }
}
