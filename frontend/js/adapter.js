const asObject = (value) => value && typeof value === 'object' && !Array.isArray(value) ? value : {};
const asArray = (value) => Array.isArray(value) ? value : [];
const asNumber = (value, fallback = 0) => Number.isFinite(Number(value)) ? Number(value) : fallback;
const asString = (value, fallback = '') => value == null ? fallback : String(value);

export function normalizeReport(input) {
  const report = asObject(input);
  const graph = asObject(report.graph);
  const reachability = asObject(report.reachability);
  const isolation = asObject(report.isolation);
  const optimization = asObject(report.optimization);
  const nodes = asArray(graph.nodes).map((node, index) => ({
    id: asString(node?.id, `node-${index + 1}`),
    type: asString(node?.type, 'UNKNOWN'),
    elevation: asNumber(node?.elevation),
    demand: asNumber(node?.base_demand ?? node?.demand),
    x: asNumber(node?.x), y: asNumber(node?.y),
    isSource: Boolean(node?.is_source ?? node?.isSource)
  }));
  const links = asArray(graph.links).map((link, index) => ({
    id: asString(link?.id, `link-${index + 1}`),
    from: asString(link?.from_id ?? link?.from),
    to: asString(link?.to_id ?? link?.to),
    type: asString(link?.link_type ?? link?.type, 'UNKNOWN'),
    status: asString(link?.status, 'UNKNOWN'),
    length: asNumber(link?.length), diameter: asNumber(link?.diameter),
    hasValve: Boolean(link?.has_valve ?? link?.hasValve),
    disruptionImpact: asNumber(link?.disruption_impact ?? link?.disruptionImpact)
  }));
  const sources = nodes.filter(node => node.isSource || /reservoir|tank|source/i.test(node.type)).map(node => node.id);
  const failures = asArray(isolation.failed_links).map(value => asString(value)).filter(Boolean);
  const disconnected = asArray(reachability.disconnected_nodes).map(value => asString(value?.id ?? value)).filter(Boolean);
  const stuckValves = asArray(isolation.stuck_valves ?? report.stuck_valves).map(value => asString(value?.id ?? value)).filter(Boolean);
  const disruptedDemand = Number.isFinite(Number(isolation.total_disrupted_demand))
    ? Number(isolation.total_disrupted_demand) : asNumber(reachability.disconnected_demand);
  const placement = (value) => {
    const result = asObject(value);
    return { algorithm: asString(result.algorithm), placedValves: asArray(result.placed_valves).map(asString),
      placedCount: asNumber(result.placed_count), totalDisruption: asNumber(result.total_disruption),
      baselineDisruption: asNumber(result.baseline_disruption), improvementPct: asNumber(result.improvement_pct),
      candidatesEvaluated: asNumber(result.candidates_evaluated), runtimeMs: asNumber(result.runtime_ms) };
  };
  const proposedValves = new Set(asArray(report.proposed_valves ?? optimization.proposed_valves).map(asString));
  for (const key of ['greedy', 'random', 'topology', 'exhaustive']) asArray(optimization[key]?.placed_valves).map(asString).forEach(value => proposedValves.add(value));
  return {
    nodes, links, valves: links.filter(link => link.hasValve || link.type.toUpperCase() === 'VALVE'), sources,
    failures, disconnected, stuckValves, proposedValves: [...proposedValves], disruptedDemand,
    isolation: { failedLinks: failures, valvesToClose: asArray(isolation.valves_to_close).map(asString),
      extraDisrupted: asArray(isolation.extra_disrupted).map(asString), totalDisruptedDemand: disruptedDemand },
    optimization: { budget: asNumber(optimization.budget), greedy: placement(optimization.greedy), random: placement(optimization.random),
      topology: placement(optimization.topology), exhaustive: placement(optimization.exhaustive) },
    structural: asObject(report.structural), rankedLinks: asArray(report.ranked_links)
  };
}

export async function readJsonFile(file) {
  return normalizeReport(JSON.parse(await file.text()));
}
