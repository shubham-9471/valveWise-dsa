/**
 * @file main.cpp
 * @brief ValveWise entry point — demonstrates both EPANET-only and integrated DSA+EPANET pipelines.
 *
 * Mode A (default): Uses IntegratedAnalysis to run the full wired pipeline:
 *   inp_parser → graph/isolation/optimizer (C DSA) → EpanetEngine (C++ hydraulics)
 *
 * Mode B (--epanet-only): Uses EpanetEngine directly without the DSA backend,
 *   equivalent to the previous standalone EPANET demo.
 *
 * Usage:
 *   valvewise-sim [<path.inp>] [--epanet-only]
 *
 * @author ValveWise Development Team
 * @date 2026
 */

#include "EpanetEngine.hpp"
#include "ValveWiseIntegrated.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string resolveInputPath(int argc, char *argv[]) {
    // Skip flag arguments when looking for the .inp path
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--", 0) != 0) {   // not a flag
            return a;
        }
    }
    // Default search paths
    for (const char *p : {"data/net_image3.inp", "../data/net_image3.inp",
                          "data/sample.inp",      "../data/sample.inp"}) {
        std::ifstream f(p);
        if (f.good()) return p;
    }
    return "data/net_image3.inp";
}

static bool hasFlag(int argc, char *argv[], const char *flag) {
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == flag)
            return true;
    return false;
}

// ── EPANET-only demo (kept for regression / quick smoke test) ─────────────────
static int runEpanetOnlyDemo(const std::string &inpPath) {
    std::cout << "\n[Mode B] Running EPANET-only engine demo...\n";

    ValveWise::EpanetEngine engine;
    if (!engine.open(inpPath, "", "")) {
        std::cerr << "[Error] Failed to open: " << inpPath << "\n";
        return 1;
    }
    engine.printNetworkSummary(std::cout);

    std::cout << "[Step] Running EN_solveH...\n";
    if (!engine.solveH()) {
        std::cerr << "[Error] Hydraulic solver failed.\n";
        return 1;
    }
    engine.printHydraulicResults(std::cout);

    std::cout << "[Step] Step-by-step time loop...\n";
    int stepCount = 0;
    engine.runHydraulicsStepByStep([&](long t, long dt) {
        ++stepCount;
        if ((t / 3600) % 6 == 0 || dt == 0)
            std::cout << "  Hour " << (t / 3600) << ":00  dt="
                      << (dt / 3600.0) << " hr\n";
    });
    std::cout << "  Completed " << stepCount << " timesteps.\n\n";

    // Close P1 / re-solve demo
    std::cout << "[Step] Closing P1 (burst pipe simulation)...\n";
    if (engine.closeLink("P1")) {
        engine.solveH();
        std::cout << "  P1 flow = " << engine.getLinkFlow("P1") << " gpm (should be 0)\n";
        std::cout << "  P3 flow = " << engine.getLinkFlow("P3") << " gpm (bypass)\n";
        engine.openLink("P1");
    }

    // Error-code demo
    double dummy = 0.0;
    int err = EN_getnodevalue(engine.getProjectHandle(), 9999, EN_PRESSURE, &dummy);
    if (err > 100)
        std::cout << "[Step] Caught expected error (" << err << "): "
                  << engine.getErrorMessage(err) << "\n";

    engine.close();
    return 0;
}

// ── Integrated DSA + EPANET pipeline demo ────────────────────────────────────
static int runIntegratedDemo(const std::string &inpPath) {
    std::cout << "\n[Mode A] Running integrated C-DSA + EPANET pipeline...\n\n";

    ValveWise::IntegratedOptions opts;
    opts.budget    = 3;
    opts.seed      = 42;
    opts.verbose   = true;
    opts.jsonOutput = "";           // set to "output.json" to serialize

    ValveWise::IntegratedAnalysis analysis(inpPath, opts);

    std::cout << "--- loadNetwork() ---\n";
    if (!analysis.loadNetwork()) {
        std::cerr << "[Error] loadNetwork() failed.\n";
        return 1;
    }

    std::cout << "\n--- solveBaselineHydraulics() ---\n";
    if (!analysis.solveBaselineHydraulics()) {
        std::cerr << "[Error] Baseline hydraulic solve failed.\n";
        return 1;
    }

    std::cout << "\n--- runFailureScenario({\"P1\"}) ---\n";
    auto result = analysis.runFailureScenario({"P1"});
    analysis.printResult(result, std::cout);

    if (!result.success) {
        std::cerr << "[Error] Scenario pipeline failed.\n";
        return 1;
    }

    std::cout << "[Mode A] Integrated pipeline completed successfully.\n";
    return 0;
}

// ── main ─────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
    std::cout << "==============================================================\n";
    std::cout << "   ValveWise: DSA + OWA-EPANET 2.2+ Integrated Engine v1.0  \n";
    std::cout << "==============================================================\n";

    std::string inpPath = resolveInputPath(argc, argv);
    std::cout << "Network: " << inpPath << "\n";

    bool epanetOnly = hasFlag(argc, argv, "--epanet-only");

    int rc = epanetOnly ? runEpanetOnlyDemo(inpPath)
                        : runIntegratedDemo(inpPath);

    std::cout << "==============================================================\n";
    std::cout << "   ValveWise finished (exit " << rc << ")\n";
    std::cout << "==============================================================\n";
    return rc;
}
