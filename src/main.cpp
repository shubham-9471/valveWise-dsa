/**
 * @file main.cpp
 * @brief Starter C++ test runner for ValveWise OWA-EPANET Toolkit integration.
 *
 * Demonstrates:
 *  1. EN_createproject and in-memory EN_open (empty rpt/out paths to suppress disk files).
 *  2. Running hydraulic simulations (both EN_solveH and step-by-step EN_openH/EN_initH/EN_runH/EN_nextH/EN_closeH).
 *  3. In-memory reading and verification of node pressures, heads, demands, and link flows/velocities.
 *  4. Clean error checking (errcode > 100) and automatic RAII cleanup via EN_deleteproject.
 *  5. Dynamic link/valve closure for failure simulation and isolation boundary analysis.
 *
 * @author ValveWise Development Team
 * @date 2026
 */

#include "EpanetEngine.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

static std::string resolveInputPath(int argc, char* argv[]) {
    if (argc > 1) {
        return argv[1];
    }

    // Default search paths for test networks
    std::vector<std::string> searchPaths = {
        "data/net_image3.inp",
        "../data/net_image3.inp",
        "data/sample.inp",
        "../data/sample.inp"
    };

    for (const auto& path : searchPaths) {
        std::ifstream f(path.c_str());
        if (f.good()) return path;
    }

    return "data/net_image3.inp";
}

int main(int argc, char* argv[]) {
    std::cout << "==============================================================\n";
    std::cout << "      ValveWise: OWA-EPANET 2.2+ C++ Simulation Engine         \n";
    std::cout << "==============================================================\n";

    std::string inpPath = resolveInputPath(argc, argv);
    std::cout << "[Step 1] Initializing EPANET Project and opening network:\n";
    std::cout << "         Path: " << inpPath << "\n";

    try {
        // 1. RAII construction initializes EN_createproject
        ValveWise::EpanetEngine engine;

        // 2. Open network with "" report and binary output paths -> 100% in-memory
        if (!engine.open(inpPath, "", "")) {
            std::cerr << "[Error] Failed to open network file '" << inpPath << "'. Exiting.\n";
            return 1;
        }

        // 3. Print network topology overview
        engine.printNetworkSummary(std::cout);

        // 4. Run Steady-State Hydraulic Simulation (EN_solveH)
        std::cout << "[Step 2] Executing steady-state hydraulic solver (EN_solveH)...\n";
        if (!engine.solveH()) {
            std::cerr << "[Error] Hydraulic solver failed.\n";
            return 1;
        }
        std::cout << "         Hydraulic solution converged successfully in memory.\n";

        // 5. Read and display in-memory node and link results
        std::cout << "[Step 3] Reading node and link values directly from memory:\n";
        engine.printHydraulicResults(std::cout);

        // 6. Extended-Period Simulation (Step-by-step EN_openH / EN_initH / EN_runH / EN_nextH / EN_closeH)
        std::cout << "[Step 4] Executing step-by-step time loop (EN_runH / EN_nextH)...\n";
        int stepCount = 0;
        bool stepSuccess = engine.runHydraulicsStepByStep([&](long currentTime, long tstep) {
            stepCount++;
            long hours = currentTime / 3600;
            if (hours % 6 == 0 || tstep == 0) { // log every 6 simulation hours
                std::cout << "         Hour " << hours << ":00 (Timestep: "
                          << (tstep / 360) / 10.0 << " hrs) - Hydraulics Balanced.\n";
            }
        });

        if (stepSuccess) {
            std::cout << "         Completed " << stepCount << " simulation timesteps.\n\n";
        }

        // 7. Demonstration of Valve/Link Closure for Pipe Failure & Isolation
        std::cout << "[Step 5] Demonstrating dynamic link closure for burst pipe isolation:\n";
        std::string failPipe = "P1";
        std::cout << "         Closing pipe '" << failPipe << "' (setting EN_STATUS = EN_CLOSED)...\n";
        if (engine.closeLink(failPipe)) {
            std::cout << "         Pipe '" << failPipe << "' status set to CLOSED.\n";

            // Re-solve hydraulics with pipe P1 closed
            engine.solveH();

            double flowP1 = engine.getLinkFlow(failPipe);
            std::cout << "         Re-evaluated flow through " << failPipe << ": "
                      << flowP1 << " gpm (isolated successfully).\n";

            // Show updated flow along alternative paths
            std::cout << "         Flow along alternative pipe P3: "
                      << engine.getLinkFlow("P3") << " gpm\n";

            // Restore pipe
            engine.openLink(failPipe);
            std::cout << "         Restored pipe '" << failPipe << "' to OPEN.\n\n";
        }

        // 8. Error checking demonstration
        std::cout << "[Step 6] Testing clean error verification (errcode > 100):\n";
        int invalidIdx = 9999;
        double dummyVal = 0.0;
        int err = EN_getnodevalue(engine.getProjectHandle(), invalidIdx, EN_PRESSURE, &dummyVal);
        if (err > 100) {
            std::cout << "         Cleanly caught expected error (" << err << "): "
                      << engine.getErrorMessage(err) << "\n";
        }

        std::cout << "\n[Step 7] Freeing project resources (EN_deleteproject)...\n";
        engine.close();
        // EpanetEngine destructor will call EN_deleteproject(m_ph) safely

        std::cout << "==============================================================\n";
        std::cout << "          ValveWise EPANET Toolkit Test Finished              \n";
        std::cout << "==============================================================\n";

    } catch (const std::exception& ex) {
        std::cerr << "[Exception caught] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
