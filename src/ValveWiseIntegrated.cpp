/**
 * @file ValveWiseIntegrated.cpp
 * @brief Bridges the C backend DSA pipeline (graph, isolation, optimizer, inp_parser)
 *        with the C++ EpanetEngine hydraulic wrapper.
 *
 * Flow:
 *   1. Parse the .inp file into a C Graph using inp_parser (DSA-based).
 *   2. Simulate failures, compute reachability, run Tarjan bridge detection.
 *   3. Compute isolation boundaries and run optimization under a budget.
 *   4. Reflect isolation valve-close decisions back into EpanetEngine
 *      (EN_setlinkvalue → EN_CLOSED) so hydraulic re-solve shows real pressures.
 *   5. All work stays in memory — no .rpt or .out files are written.
 *
 * @author ValveWise Development Team
 * @date 2026
 */

#include "ValveWiseIntegrated.hpp"

// C backend DSA modules (C linkage already declared by extern "C" guards in headers)
#include "graph.h"
#include "hashmap.h"
#include "inp_parser.h"
#include "isolation.h"
#include "json_output.h"
#include "optimizer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

namespace ValveWise {

// ─────────────────────────────────────────────────────────────────────────────
// IntegratedAnalysis  (member-function implementations)
// ─────────────────────────────────────────────────────────────────────────────

IntegratedAnalysis::IntegratedAnalysis(const std::string &inpFile,
                                       const IntegratedOptions &opts)
    : m_inpFile(inpFile), m_opts(opts),
      m_engine(std::make_unique<EpanetEngine>()) {}

IntegratedAnalysis::~IntegratedAnalysis() {
    if (m_netIdx) { network_index_free(m_netIdx); m_netIdx = nullptr; }
    if (m_graph)  { graph_free(m_graph);           m_graph  = nullptr; }
    // EpanetEngine is destroyed by its own RAII destructor (unique_ptr)
}

// ─── Step 1: Parse and open the network ──────────────────────────────────────
bool IntegratedAnalysis::loadNetwork() {
    // Open the hydraulic engine (in-memory — no .rpt or .out files)
    if (!m_engine->open(m_inpFile, "", "")) {
        std::cerr << "[Integration] Failed to open EPANET project for '"
                  << m_inpFile << "'\n";
        return false;
    }

    // Also parse into the C DSA Graph (independent of EPANET runtime)
    m_graph = graph_create(64, 128);
    m_netIdx = network_index_create();
    if (!m_graph || !m_netIdx) {
        std::cerr << "[Integration] Memory allocation failed for C graph.\n";
        return false;
    }

    if (parse_inp_file(m_inpFile.c_str(), m_graph, m_netIdx) != 0) {
        std::cerr << "[Integration] inp_parser failed on '" << m_inpFile << "'\n";
        return false;
    }

    if (m_opts.verbose) {
        graph_print_summary(m_graph);
        m_engine->printNetworkSummary(std::cout);
    }
    return true;
}

// ─── Step 2: Baseline hydraulic solve (all links open) ───────────────────────
bool IntegratedAnalysis::solveBaselineHydraulics() {
    if (!m_engine->isOpen()) {
        std::cerr << "[Integration] Network not loaded — call loadNetwork() first.\n";
        return false;
    }
    bool ok = m_engine->solveH();
    if (ok && m_opts.verbose) {
        std::cout << "[Integration] Baseline hydraulic solve complete.\n";
        m_engine->printHydraulicResults(std::cout);
    }
    return ok;
}

// ─── Step 3: Simulate failure scenarios & run isolation/optimization ──────────
IntegratedResult IntegratedAnalysis::runFailureScenario(
    const std::vector<std::string> &failLinkIds,
    const std::vector<std::string> &stuckValveIds) {

    IntegratedResult result;
    result.success = false;

    if (!m_graph || !m_netIdx) {
        std::cerr << "[Integration] Network not loaded.\n";
        return result;
    }

    // ── 3a. Resolve fail-link string IDs to integer link indices ─────────────
    std::vector<int> failIdxList;
    for (const auto &lid : failLinkIds) {
        int idx = -1;
        if (hashmap_get(m_netIdx->link_map, lid.c_str(), &idx)) {
            failIdxList.push_back(idx);
            if (m_opts.verbose)
                std::cout << "[Integration] Failing link: " << lid
                          << " (C-graph idx " << idx << ")\n";
        } else {
            std::cerr << "[Integration] WARNING: Link '" << lid
                      << "' not found in C graph — skipping.\n";
        }
    }

    // ── 3b. Mark stuck valves in the C graph ─────────────────────────────────
    for (const auto &vid : stuckValveIds) {
        int idx = -1;
        if (hashmap_get(m_netIdx->link_map, vid.c_str(), &idx)) {
            m_graph->links[idx].status = STATUS_STUCK;
            if (m_opts.verbose)
                std::cout << "[Integration] Stuck valve: " << vid
                          << " (C-graph idx " << idx << ")\n";
        }
    }

    // ── 3c. Simulate burst failures (mark STATUS_BURST in C graph) ───────────
    for (int fidx : failIdxList) {
        simulate_failure(m_graph, fidx);
    }

    // ── 3d. BFS reachability — which junctions are disconnected? ─────────────
    ReachabilityResult *reach = compute_reachability(m_graph);
    if (!reach) {
        std::cerr << "[Integration] Reachability computation failed.\n";
        return result;
    }

    // ── 3e. Isolation boundary — which valves must close? ────────────────────
    IsolationResult *isol = nullptr;
    if (!failIdxList.empty()) {
        isol = compute_isolation(m_graph, failIdxList.data(),
                                 (int)failIdxList.size());
    }

    // ── 3f. Tarjan bridge/articulation-point structural analysis ─────────────
    StructuralResult *struc = compute_structural_analysis(m_graph);

    // ── 3g. Rank links by criticality ────────────────────────────────────────
    //  rank_links signature: (const Graph*, const StructuralResult*, RankMode, int*)
    int ranked_count = 0;
    RankedLink *ranked = nullptr;
    if (struc) {
        ranked = rank_links(m_graph, struc, RANK_BY_DISRUPTION, &ranked_count);
    }

    // ── 3h. Optimization: greedy + random + topology + exhaustive ────────────
    OptimizationReport *optReport = nullptr;
    if (!failIdxList.empty()) {
        optReport = run_full_optimization(m_graph, m_opts.budget,
                                          failIdxList.data(),
                                          (int)failIdxList.size(),
                                          struc, m_opts.seed);
    }

    // ── 3i. Collect DSA results for caller ───────────────────────────────────
    result.disconnectedJunctions = reach ? reach->disconnected_count : 0;
    result.disconnectedDemandLps = reach ? reach->disconnected_demand : 0.0;
    result.bridgeCount           = struc ? struc->bridge_count : 0;
    result.articulationPointCount = struc ? struc->ap_count : 0;

    if (isol) {
        for (int i = 0; i < isol->valve_count; ++i) {
            int vi = isol->valves_to_close[i];
            if (vi >= 0 && vi < m_graph->link_count) {
                result.valvesToClose.push_back(
                    std::string(m_graph->links[vi].id));
            }
        }
        result.extraDisruptedByStuck = isol->extra_disrupted_count;
        result.totalDisruptedDemand  = isol->total_disrupted_demand;
    }

    // ── 3j. Reflect valve-close decisions into EpanetEngine ─────────────────
    //        Close valves identified by DSA isolation in the hydraulic solver,
    //        then re-solve to get realistic post-failure pressures in memory.
    if (!result.valvesToClose.empty()) {
        if (m_opts.verbose)
            std::cout << "[Integration] Closing " << result.valvesToClose.size()
                      << " isolation valve(s) in EPANET engine...\n";
        for (const auto &vid : result.valvesToClose) {
            m_engine->closeLink(vid);
        }
    }

    // Also close the failed pipes in the EPANET engine
    for (const auto &fid : failLinkIds) {
        m_engine->closeLink(fid);
    }

    // Re-solve hydraulics with failure + isolation valves applied
    bool hydOk = m_engine->solveH();
    if (hydOk && m_opts.verbose) {
        std::cout << "[Integration] Post-failure hydraulic results:\n";
        m_engine->printHydraulicResults(std::cout);
    }

    // Store post-failure hydraulic snapshot
    result.postFailureNodes = m_engine->getAllNodeResults();
    result.postFailureLinks = m_engine->getAllLinkResults();

    // ── 3k. Serialize JSON output if requested ───────────────────────────────
    if (!m_opts.jsonOutput.empty()) {
        char *json = json_full_report(m_graph, reach, isol, struc,
                                      ranked, ranked_count, optReport);
        if (json) {
            json_write_to_file(m_opts.jsonOutput.c_str(), json);
            if (m_opts.verbose)
                std::cout << "[Integration] Results written to: "
                          << m_opts.jsonOutput << "\n";
            json_free(json);
        }
    }

    // ── 3l. Reset graph state for next scenario ───────────────────────────────
    reset_failures(m_graph);
    // Restore stuck valves to open
    for (const auto &vid : stuckValveIds) {
        int idx = -1;
        if (hashmap_get(m_netIdx->link_map, vid.c_str(), &idx)) {
            m_graph->links[idx].status = STATUS_OPEN;
        }
    }
    // Restore EPANET engine links
    for (const auto &fid : failLinkIds) {
        m_engine->openLink(fid);
    }
    for (const auto &vid : result.valvesToClose) {
        m_engine->openLink(vid);
    }

    // Free C allocations
    if (optReport) free_optimization_report(optReport);
    if (ranked)    free(ranked);
    if (struc)     free_structural(struc);
    if (isol)      free_isolation(isol);
    if (reach)     free_reachability(reach);

    result.success = true;
    return result;
}

// ─── Step 4: Print a human-readable summary ──────────────────────────────────
void IntegratedAnalysis::printResult(const IntegratedResult &r,
                                     std::ostream &os) const {
    os << "\n════════════════════════════════════════════════════\n";
    os << "            ValveWise Integrated Analysis Report\n";
    os << "════════════════════════════════════════════════════\n";
    os << std::left;
    os << "  Disconnected junctions : " << r.disconnectedJunctions
       << " (" << std::fixed << std::setprecision(2)
       << r.disconnectedDemandLps << " L/s demand)\n";
    os << "  Isolation valves to close: ";
    if (r.valvesToClose.empty()) {
        os << "(none)\n";
    } else {
        for (const auto &v : r.valvesToClose) os << v << " ";
        os << "\n";
    }
    os << "  Extra disrupted by stuck valves : "
       << r.extraDisruptedByStuck << "\n";
    os << "  Total disrupted demand  : " << r.totalDisruptedDemand << " L/s\n";
    os << "  Bridges detected        : " << r.bridgeCount << "\n";
    os << "  Articulation points     : " << r.articulationPointCount << "\n";
    os << "  Post-failure hydraulics :\n";

    os << "    Nodes:\n";
    for (const auto &n : r.postFailureNodes) {
        os << "      " << std::setw(8) << n.id
           << "  P=" << std::setw(7) << n.pressure << " psi"
           << "  H=" << std::setw(8) << n.head << " ft\n";
    }
    os << "    Links:\n";
    for (const auto &l : r.postFailureLinks) {
        os << "      " << std::setw(8) << l.id
           << "  Status=" << (l.status == EN_CLOSED ? "CLOSED" : "OPEN  ")
           << "  Flow=" << std::setw(10) << l.flow << " gpm\n";
    }
    os << "════════════════════════════════════════════════════\n\n";
}

} // namespace ValveWise
