const asObject = value => value && typeof value === 'object' && !Array.isArray(value) ? value : {};
const asArray = value => Array.isArray(value) ? value : [];
const asNumber = (value, fallback = 0) => Number.isFinite(Number(value)) ? Number(value) : fallback;
const asString = (value, fallback = '') => value == null ? fallback : String(value);
const optionalNumber = value => Number.isFinite(Number(value)) ? Number(value) : null;
const hydraulicState = value => {
  const state = asObject(value);
  return {
    nodes: asArray(state.nodes).map(node => ({ id: asString(node?.id), pressure: optionalNumber(node?.pressure), demand: optionalNumber(node?.demand), served: node?.served == null ? null : Boolean(node.served) })),
    links: asArray(state.links).map(link => ({ id: asString(link?.id), flow: optionalNumber(link?.flow), status: asString(link?.status) })),
    pressureDeficient: asArray(state.pressure_deficient).map(value => asString(value?.id ?? value)).filter(Boolean),
    unservedDemand: optionalNumber(state.unserved_demand)
  };
};

const normalizePlacement = value => {
  const result = asObject(value);
  return {
    algorithm: asString(result.algorithm),
    placedValves: asArray(result.placed_valves).map(asString),
    placedCount: asNumber(result.placed_count),
    totalDisruption: asNumber(result.total_disruption),
    baselineDisruption: asNumber(result.baseline_disruption),
    meanDisruption: asNumber(result.mean_disruption),
    bestDisruption: asNumber(result.best_disruption),
    improvementPct: asNumber(result.improvement_pct),
    candidatesEvaluated: asNumber(result.candidates_evaluated),
    runtimeMs: asNumber(result.runtime_ms),
    seed: asNumber(result.seed),
    trials: asNumber(result.trials),
    skipped: Boolean(result.skipped),
    skippedReason: asString(result.skip_reason ?? result.reason),
    placementExplanations: asArray(result.placement_explanations).map(item => {
      const explanation = asObject(item);
      return { valve: asString(explanation.valve), before: asNumber(explanation.before), after: asNumber(explanation.after), why: asString(explanation.why) };
    })
  };
};

const normalizeScenario = value => {
  const scenario = asObject(value);
  return {
    scenario: asNumber(scenario.scenario),
    failedLinks: asArray(scenario.failed_links).map(asString),
    valvesToClose: asArray(scenario.valves_to_close).map(item => {
      const valve = asObject(item);
      return { link: asString(valve.link ?? valve.id), reason: asString(valve.reason) };
    }),
    stuckBoundary: asArray(scenario.stuck_boundary).map(item => {
      const valve = asObject(item);
      return { link: asString(valve.link ?? valve.id), reason: asString(valve.reason) };
    }),
    disconnectedJunctions: asArray(scenario.disconnected_junctions).map(asString),
    alternativeRoutes: asArray(scenario.alternative_routes).map(route => ({
      source: asString(route?.source), viaLinks: asArray(route?.via_links).map(asString)
    })),
    isolationFeasible: typeof scenario.isolation_feasible === 'boolean' ? scenario.isolation_feasible : null
  };
};

export function normalizeReport(input) {
  const report = asObject(input);
  const graph = asObject(report.graph);
  const reachability = asObject(report.reachability);
  const isolation = asObject(report.isolation);
  const optimization = asObject(report.optimization);
  const performance = asObject(report.performance);
  const hydraulicRoot = asObject(report.hydraulics);
  const explanationsRoot = asObject(report.explanations);
  const scenarios = asArray(explanationsRoot.scenarios).map(normalizeScenario);
  const nodes = asArray(graph.nodes).map((node, index) => ({
    id: asString(node?.id, `node-${index + 1}`), type: asString(node?.type, 'UNKNOWN'),
    elevation: asNumber(node?.elevation), demand: asNumber(node?.base_demand ?? node?.demand),
    x: asNumber(node?.x), y: asNumber(node?.y), isSource: Boolean(node?.is_source ?? node?.isSource)
  }));
  const links = asArray(graph.links).map((link, index) => ({
    id: asString(link?.id, `link-${index + 1}`), from: asString(link?.from_id ?? link?.from),
    to: asString(link?.to_id ?? link?.to), type: asString(link?.link_type ?? link?.type, 'UNKNOWN'),
    status: asString(link?.status, 'UNKNOWN'), length: asNumber(link?.length), diameter: asNumber(link?.diameter),
    hasValve: Boolean(link?.has_valve ?? link?.hasValve), disruptionImpact: asNumber(link?.disruption_impact ?? link?.disruptionImpact)
  }));
  const sources = nodes.filter(node => node.isSource || /reservoir|tank|source/i.test(node.type)).map(node => node.id);
  const failures = asArray(isolation.failed_links).map(asString).filter(Boolean);
  const disconnected = asArray(reachability.disconnected_nodes).map(value => asString(value?.id ?? value)).filter(Boolean);
  const firstScenario = scenarios[0] ?? {};
  const stuckValves = (firstScenario.stuckBoundary ?? []).map(item => item.link);
  const disruptedDemand = Number.isFinite(Number(isolation.total_disrupted_demand)) ? Number(isolation.total_disrupted_demand) : asNumber(reachability.disconnected_demand);
  const proposedValves = new Set();
  for (const key of ['greedy', 'random', 'topology', 'exhaustive']) asArray(optimization[key]?.placed_valves).map(asString).forEach(value => proposedValves.add(value));
  const runs = Object.fromEntries(['greedy', 'random', 'topology', 'exhaustive'].map(key => [key, normalizePlacement(optimization[key])]));
  const optimizationRuntimeMs = asNumber(performance.optimization_ms, Object.values(runs).reduce((sum, run) => sum + run.runtimeMs, 0));
  const candidatesEvaluated = Object.values(runs).reduce((sum, run) => sum + run.candidatesEvaluated, 0);
  const rawError = report.error;
  const error = rawError == null ? null : typeof rawError === 'string' ? { message: rawError } : asObject(rawError);
  const hydraulicStates = Object.fromEntries(['baseline', 'after_failure', 'after_isolation', 'after_optimization'].map(key => [key, hydraulicState(hydraulicRoot[key])]));
  return {
    nodes, links, valves: links.filter(link => link.hasValve || link.type.toUpperCase() === 'VALVE'), sources,
    failures, disconnected, stuckValves, proposedValves: [...proposedValves], disruptedDemand,
    reachability: { pressureDeficientCount: optionalNumber(reachability.pressure_deficient_count ?? reachability.pressure_deficient) },
    isolation: {
      failedLinks: failures, valvesToClose: asArray(isolation.valves_to_close).map(asString),
      extraDisrupted: asArray(isolation.extra_disrupted).map(asString), affectedSections: [],
      pressureDeficientCount: optionalNumber(isolation.pressure_deficient_count ?? isolation.pressure_deficient),
      isolationFeasible: typeof isolation.isolation_feasible === 'boolean' ? isolation.isolation_feasible : firstScenario.isolationFeasible ?? null,
      totalDisruptedDemand: disruptedDemand
    },
    explanations: { scenarios },
    optimization: { budget: asNumber(optimization.budget), seed: asNumber(optimization.seed), scenarioCount: asNumber(optimization.scenario_count), ...runs },
    structural: asObject(report.structural),
    rankedLinks: asArray(report.ranked_links).map((row, index) => ({ linkId: asString(row?.link_id ?? row?.id, `link-${index + 1}`), score: asNumber(row?.score), reason: asString(row?.reason) })),
    performance: {
      bfsMs: asNumber(performance.bfs_ms ?? performance.bfs_time_ms, asNumber(reachability.bfs_ms ?? reachability.bfs_time_ms)), optimizationRuntimeMs,
      candidates: candidatesEvaluated, memoryKb: asNumber(performance.memory_kb, Number.NaN),
      memory: Number.isFinite(Number(performance.memory_kb)) ? Number(performance.memory_kb) : performance.memory_bytes ?? null
    },
    error,
    hydraulics: { available: typeof hydraulicRoot.available === 'boolean' ? hydraulicRoot.available : false,
      engine: asString(hydraulicRoot.engine), units: { pressure: asString(hydraulicRoot.units?.pressure), flow: asString(hydraulicRoot.units?.flow) },
      minPressure: optionalNumber(hydraulicRoot.min_pressure), states: hydraulicStates }
  };
}

export async function readJsonFile(file) {
  return normalizeReport(JSON.parse(await file.text()));
}
