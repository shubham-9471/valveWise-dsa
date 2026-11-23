/**
 * @file ValveWiseIntegrated.hpp
 * @brief High-level bridge class wiring the C DSA backend with the C++ EpanetEngine.
 *
 * IntegratedAnalysis owns both a C Graph (DSA) and an EpanetEngine (hydraulics),
 * keeping all state in-memory — no .rpt or .out files are ever written.
 *
 * Usage:
 *   ValveWise::IntegratedOptions opts;
 *   opts.budget = 3;
 *   opts.verbose = true;
 *   ValveWise::IntegratedAnalysis analysis("network.inp", opts);
 *   analysis.loadNetwork();
 *   analysis.solveBaselineHydraulics();
 *   auto result = analysis.runFailureScenario({"P1"}, {"V1"});
 *   analysis.printResult(result);
 *
 * @author ValveWise Development Team
 * @date 2026
 */

#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "EpanetEngine.hpp"  // brings in NodeResult, LinkResult, EpanetEngine

// C types — Graph is already typedef'd in graph.h (included via EpanetEngine.hpp).
// NetworkIndex is typedef'd in inp_parser.h — include it here.
#include "inp_parser.h"

namespace ValveWise {

// ─── Options struct ──────────────────────────────────────────────────────────

/**
 * @struct IntegratedOptions
 * @brief Runtime configuration for an IntegratedAnalysis session.
 */
struct IntegratedOptions {
    int         budget    = 3;     ///< Max valves to close in optimizer
    unsigned int seed     = 42;    ///< RNG seed for random optimizer
    bool         verbose  = false; ///< Print detailed progress to stdout
    std::string  jsonOutput = "";  ///< If non-empty, serialize results here
};

// ─── Result struct ───────────────────────────────────────────────────────────

/**
 * @struct IntegratedResult
 * @brief Results from one failure scenario run through the full pipeline.
 */
struct IntegratedResult {
    bool success                 = false; ///< true if pipeline completed without error
    int  disconnectedJunctions   = 0;     ///< Junctions cut off from supply
    double disconnectedDemandLps = 0.0;   ///< Demand lost (L/s)
    std::vector<std::string> valvesToClose;    ///< Valve IDs the DSA says to close
    int    extraDisruptedByStuck = 0;     ///< Extra nodes disrupted by stuck valves
    double totalDisruptedDemand  = 0.0;   ///< Total disrupted demand after isolation (L/s)
    int    bridgeCount           = 0;     ///< Bridge links found by Tarjan
    int    articulationPointCount = 0;    ///< Articulation points found
    std::vector<NodeResult> postFailureNodes; ///< Post-failure hydraulic node snapshot
    std::vector<LinkResult> postFailureLinks; ///< Post-failure hydraulic link snapshot
};

// ─── IntegratedAnalysis class ────────────────────────────────────────────────

/**
 * @class IntegratedAnalysis
 * @brief End-to-end failure-analysis pipeline combining C DSA and hydraulic simulation.
 *
 * Owns a C Graph (populated by inp_parser) and an EpanetEngine.  Call:
 *   loadNetwork()             → parse DSA graph + open EPANET
 *   solveBaselineHydraulics() → steady-state solve with all links open
 *   runFailureScenario(...)   → DSA isolation + hydraulic re-solve (in-memory)
 *   printResult(...)          → human-readable summary
 */
class IntegratedAnalysis {
public:
    /**
     * @brief Construct the integrated analysis object.
     * @param inpFile  Absolute or relative path to the EPANET .inp file.
     * @param opts     Runtime options (budget, seed, verbose, etc.)
     */
    explicit IntegratedAnalysis(const std::string &inpFile,
                                const IntegratedOptions &opts = {});

    /** @brief Destructor — frees C graph and EPANET project. */
    ~IntegratedAnalysis();

    // Non-copyable (EPANET project handle is not safe to copy)
    IntegratedAnalysis(const IntegratedAnalysis &) = delete;
    IntegratedAnalysis &operator=(const IntegratedAnalysis &) = delete;

    /**
     * @brief Parse .inp into C DSA Graph and open EPANET in-memory.
     * @return true on success.
     */
    bool loadNetwork();

    /**
     * @brief Run a steady-state hydraulic solve with all links open.
     * @return true on success.
     */
    bool solveBaselineHydraulics();

    /**
     * @brief Full failure-analysis pipeline for one scenario.
     *
     * Steps:
     *  1. Mark stuck valves in C graph.
     *  2. Simulate burst for each failLinkId.
     *  3. BFS reachability + Tarjan bridge detection.
     *  4. Compute isolation boundary.
     *  5. Run optimizer (greedy + random + topology + exhaustive).
     *  6. Reflect valve-close decisions into EpanetEngine.
     *  7. Re-solve hydraulics in memory.
     *  8. Collect post-failure node/link snapshots.
     *  9. Optionally serialize JSON.
     * 10. Reset graph + EPANET state for next scenario.
     *
     * @param failLinkIds   IDs of burst pipes (from .inp [PIPES] section).
     * @param stuckValveIds IDs of stuck-open valves (cannot be closed).
     * @return Populated IntegratedResult; result.success == false on error.
     */
    IntegratedResult runFailureScenario(
        const std::vector<std::string> &failLinkIds,
        const std::vector<std::string> &stuckValveIds = {});

    /**
     * @brief Print a human-readable summary of an IntegratedResult.
     * @param r   The result to print.
     * @param os  Output stream (default: std::cout).
     */
    void printResult(const IntegratedResult &r,
                     std::ostream &os = std::cout) const;

    /** @brief Direct access to the EPANET hydraulic engine. */
    EpanetEngine &engine() { return *m_engine; }

private:
    std::string              m_inpFile;
    IntegratedOptions        m_opts;
    std::unique_ptr<EpanetEngine> m_engine;
    Graph        *m_graph  = nullptr;
    NetworkIndex *m_netIdx = nullptr;
};

} // namespace ValveWise
