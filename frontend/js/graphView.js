const SVG_NS = 'http://www.w3.org/2000/svg';
const finite = value => Number.isFinite(Number(value));

export function nodeTypeKey(node = {}) {
  const type = String(node.type ?? '').toUpperCase().replace(/[- ]/g, '_');
  if (type.includes('RESERVOIR') || node.isSource) return 'reservoir';
  if (type.includes('TANK')) return 'tank';
  if (type.includes('PUMP')) return 'pump-end';
  return 'junction';
}

export function getNodeVisual(node = {}, state = {}) {
  const type = nodeTypeKey(node);
  const base = { type, shape: type === 'reservoir' ? 'rect' : type === 'tank' ? 'diamond' : type === 'pump-end' ? 'hexagon' : 'circle', fill: type === 'reservoir' ? '#43b7a0' : type === 'tank' ? '#d19a4a' : type === 'pump-end' ? '#b477dc' : '#5b8dd9', stroke: '#dce9e5', opacity: 1 };
  if (state.disconnected) return { ...base, fill: '#697570', stroke: '#e4efeb', opacity: 0.5, disconnected: true };
  if (state.selected) return { ...base, stroke: '#fff3a8', strokeWidth: 3 };
  return base;
}

export function getLinkVisual(link = {}, state = {}) {
  const type = String(link.type ?? '').toUpperCase();
  const valve = Boolean(link.hasValve) || type === 'VALVE';
  if (state.failed) return { stroke: '#ed5c5c', width: 4, opacity: 1, dash: '', marker: valve };
  if (state.stuck) return { stroke: '#d58bff', width: 3, opacity: 1, dash: '7 4', marker: true };
  if (state.closed) return { stroke: '#ffd166', width: 3, opacity: 1, dash: '', marker: true };
  if (state.proposed) return { stroke: '#36d399', width: 3, opacity: 1, dash: '3 3', marker: true };
  return { stroke: valve ? '#8ab8ad' : '#58736d', width: valve ? 2.5 : 1.5, opacity: 0.9, dash: '', marker: valve };
}

function svg(tag, attrs = {}) { const element = document.createElementNS(SVG_NS, tag); Object.entries(attrs).forEach(([key, value]) => element.setAttribute(key, String(value))); return element; }

function layoutNodes(nodes) {
  const hasCoordinates = nodes.length > 0 && nodes.every(node => finite(node.x) && finite(node.y)) && nodes.some(node => Number(node.x) !== 0 || Number(node.y) !== 0);
  const positions = new Map();
  if (hasCoordinates) {
    const xs = nodes.map(node => Number(node.x)); const ys = nodes.map(node => Number(node.y)); const minX = Math.min(...xs); const maxX = Math.max(...xs); const minY = Math.min(...ys); const maxY = Math.max(...ys); const scale = Math.max(maxX - minX, maxY - minY, 1);
    nodes.forEach(node => positions.set(node.id, { x: 80 + ((Number(node.x) - minX) / scale) * 840, y: 70 + ((maxY - Number(node.y)) / scale) * 560 })); return positions;
  }
  const radius = Math.max(110, Math.min(260, nodes.length * 18));
  nodes.forEach((node, index) => { const angle = (index / Math.max(nodes.length, 1)) * Math.PI * 2 - Math.PI / 2; positions.set(node.id, { x: 500 + Math.cos(angle) * radius, y: 350 + Math.sin(angle) * radius }); }); return positions;
}

function tooltipText(kind, item) { return kind === 'node' ? `${item.id}\nDemand: ${item.demand ?? 0}` : `${item.id}\nLength: ${item.length ?? 0}\nStatus: ${item.status ?? 'UNKNOWN'}`; }

export class NetworkView {
  constructor(container, onSelect = () => {}) { this.container = container; this.onSelect = onSelect; this.selection = null; this.zoom = 1; this.pan = { x: 0, y: 0 }; }
  render(model) {
    this.model = model; this.positions = layoutNodes(model.nodes); this.zoom = 1; this.pan = { x: 0, y: 0 }; this.container.replaceChildren();
    const wrapper = document.createElement('div'); wrapper.className = 'network-canvas'; const svgRoot = svg('svg', { viewBox: '0 0 1000 700', role: 'img', 'aria-label': 'ValveWise network graph' }); const graph = svg('g'); const linksLayer = svg('g', { class: 'network-links' }); const nodesLayer = svg('g', { class: 'network-nodes' }); graph.append(linksLayer, nodesLayer); svgRoot.append(graph); wrapper.append(svgRoot); const tooltip = document.createElement('div'); tooltip.className = 'network-tooltip'; tooltip.hidden = true; wrapper.append(tooltip); this.container.append(wrapper); this.svgRoot = svgRoot; this.graph = graph; this.tooltip = tooltip;
    const failed = new Set(model.failures); const closed = new Set(model.isolation?.valvesToClose ?? []); const stuck = new Set(model.stuckValves ?? []); const proposed = new Set(model.proposedValves ?? []); const disconnected = new Set(model.disconnected ?? []);
    model.links.forEach(link => { const a = this.positions.get(link.from); const b = this.positions.get(link.to); if (!a || !b) return; const visual = getLinkVisual(link, { failed: failed.has(link.id), closed: closed.has(link.id), stuck: stuck.has(link.id) || /STUCK/i.test(link.status), proposed: proposed.has(link.id) }); const line = svg('line', { x1: a.x, y1: a.y, x2: b.x, y2: b.y, stroke: visual.stroke, 'stroke-width': visual.width, 'stroke-dasharray': visual.dash, opacity: visual.opacity, 'vector-effect': 'non-scaling-stroke' }); line.addEventListener('pointerenter', event => this.showTooltip(event, 'link', link)); line.addEventListener('pointerleave', () => this.hideTooltip()); line.addEventListener('click', () => { this.selection = { kind: 'link', id: link.id }; this.onSelect(this.selection, link); }); linksLayer.append(line); if (visual.marker) { const mx = (a.x + b.x) / 2; const my = (a.y + b.y) / 2; const marker = svg('polygon', { points: `${mx},${my - 6} ${mx + 6},${my} ${mx},${my + 6} ${mx - 6},${my}`, fill: visual.stroke, class: 'valve-marker' }); marker.addEventListener('pointerenter', event => this.showTooltip(event, 'link', link)); marker.addEventListener('pointerleave', () => this.hideTooltip()); marker.addEventListener('click', () => { this.selection = { kind: 'link', id: link.id }; this.onSelect(this.selection, link); }); linksLayer.append(marker); } });
    model.nodes.forEach(node => { const point = this.positions.get(node.id); if (!point) return; const visual = getNodeVisual(node, { disconnected: disconnected.has(node.id), selected: this.selection?.id === node.id }); const group = svg('g', { class: 'network-node', transform: `translate(${point.x} ${point.y})`, opacity: visual.opacity }); let shape; if (visual.shape === 'rect') shape = svg('rect', { x: -9, y: -9, width: 18, height: 18, rx: 3 }); else if (visual.shape === 'diamond') shape = svg('polygon', { points: '0,-12 12,0 0,12 -12,0' }); else if (visual.shape === 'hexagon') shape = svg('polygon', { points: '-10,-6 -5,-11 5,-11 10,-6 5,11 -5,11' }); else shape = svg('circle', { r: 8 }); shape.setAttribute('fill', visual.fill); shape.setAttribute('stroke', visual.stroke); shape.setAttribute('stroke-width', visual.strokeWidth ?? 1.5); shape.setAttribute('vector-effect', 'non-scaling-stroke'); group.append(shape); const label = svg('text', { x: 12, y: 4 }); label.textContent = node.id; group.append(label); group.addEventListener('pointerenter', event => this.showTooltip(event, 'node', node)); group.addEventListener('pointerleave', () => this.hideTooltip()); group.addEventListener('click', () => { this.selection = { kind: 'node', id: node.id }; this.onSelect(this.selection); }); nodesLayer.append(group); });
    this.bindViewport(svgRoot); this.fit();
  }
  showTooltip(event, kind, item) { this.tooltip.textContent = tooltipText(kind, item); this.tooltip.hidden = false; const rect = this.container.getBoundingClientRect(); this.tooltip.style.left = `${event.clientX - rect.left + 12}px`; this.tooltip.style.top = `${event.clientY - rect.top + 12}px`; }
  hideTooltip() { if (this.tooltip) this.tooltip.hidden = true; }
  fit() { if (!this.svgRoot) return; this.zoom = 1; this.pan = { x: 0, y: 0 }; this.graph.setAttribute('transform', 'translate(0 0) scale(1)'); }
  bindViewport(svgRoot) { svgRoot.addEventListener('wheel', event => { event.preventDefault(); this.zoom = Math.max(0.3, Math.min(5, this.zoom * (event.deltaY < 0 ? 1.12 : 0.89))); this.graph.setAttribute('transform', `translate(${this.pan.x} ${this.pan.y}) scale(${this.zoom})`); }, { passive: false }); let start = null; svgRoot.addEventListener('pointerdown', event => { if (event.target !== svgRoot) return; start = { x: event.clientX, y: event.clientY, pan: { ...this.pan } }; svgRoot.setPointerCapture(event.pointerId); }); svgRoot.addEventListener('pointermove', event => { if (!start) return; this.pan = { x: start.pan.x + (event.clientX - start.x) / 1.2, y: start.pan.y + (event.clientY - start.y) / 1.2 }; this.graph.setAttribute('transform', `translate(${this.pan.x} ${this.pan.y}) scale(${this.zoom})`); }); svgRoot.addEventListener('pointerup', () => { start = null; }); }
}
