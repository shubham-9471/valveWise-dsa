/**
 * @file EpanetEngine.hpp
 * @brief Modern C++ RAII wrapper for the OWA-EPANET 2.2+ multi-threaded C
 * toolkit.
 *
 * Provides safe in-memory hydraulic and water-quality simulations without
 * generating intermediate report (.rpt) or binary (.out/.hyd) disk files.
 * Designed as the core hydraulic engine for ValveWise network failure and
 * valve-isolation optimization algorithms.
 *
 * @author ValveWise Development Team
 * @date 2026
 */

#ifndef VALVEWISE_EPANET_ENGINE_HPP
#define VALVEWISE_EPANET_ENGINE_HPP

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Include OWA-EPANET headers
extern "C" {
#include "epanet2_2.h"
#include "epanet2_enums.h"
}

// Include C backend headers
#include "graph.h"
#include "isolation.h"
#include "optimizer.h"

namespace ValveWise {

/**
 * @struct NodeResult
 * @brief In-memory snapshot of computed hydraulic values for a network node.
 */
struct NodeResult {
  int index;
  std::string id;
  double elevation;
  double demand;
  double head;
  double pressure;
};

/**
 * @struct LinkResult
 * @brief In-memory snapshot of computed hydraulic values for a network link.
 */
struct LinkResult {
  int index;
  std::string id;
  int linkType;
  int status;
  double flow;
  double velocity;
  double headloss;
};

/**
 * @class EpanetEngine
 * @brief Thread-safe RAII controller for an EPANET 2.2 project instance.
 */
class EpanetEngine {
public:
  /**
   * @brief Constructs an EPANET project handle.
   * @param inpFilePath Optional path to an .inp network file to open
   * immediately.
   */
  explicit EpanetEngine(const std::string &inpFilePath = "");

  /**
   * @brief Destructor. Safely closes any active file and frees the EPANET
   * project handle.
   */
  ~EpanetEngine();

  // Disable copy semantics to prevent double-freeing ph
  EpanetEngine(const EpanetEngine &) = delete;
  EpanetEngine &operator=(const EpanetEngine &) = delete;

  // Enable move semantics
  EpanetEngine(EpanetEngine &&other) noexcept;
  EpanetEngine &operator=(EpanetEngine &&other) noexcept;

  /**
   * @brief Opens an EPANET .inp file with in-memory reporting (no disk files).
   * @param inpFile Path to input .inp network file.
   * @param rptFile Path to report file (default "" to suppress .rpt
   * generation).
   * @param outFile Path to binary output file (default "" to suppress .out
   * generation).
   * @return true on success, false on error.
   */
  bool open(const std::string &inpFile, const std::string &rptFile = "",
            const std::string &outFile = "");

  /**
   * @brief Closes the currently loaded network and frees EPANET internal
   * network structures.
   * @return true on success.
   */
  bool close();

  /**
   * @brief Checks if a network is currently loaded and ready for simulation.
   */
  bool isOpen() const { return m_isOpen; }

  /**
   * @brief Runs an instantaneous (steady-state) hydraulic analysis in memory.
   * @return true on success, false if hydraulic solver encountered an error.
   */
  bool solveH();

  /**
   * @brief Runs extended-period hydraulic simulation step-by-step.
   * @param onStep Optional callback invoked after each hydraulic timestep with
   * (currentTime, tstep).
   * @return true on success.
   */
  bool runHydraulicsStepByStep(
      std::function<void(long currentTime, long tstep)> onStep = nullptr);

  /**
   * @brief Runs water-quality simulation (optional, can remain disabled).
   * @return true on success.
   */
  bool solveQ();

  // ── Network Topology Queries ─────────────────────────────────────────────
  int getNodeCount() const;
  int getLinkCount() const;

  std::string getNodeId(int index) const;
  int getNodeIndex(const std::string &id) const;

  std::string getLinkId(int index) const;
  int getLinkIndex(const std::string &id) const;

  // ── In-Memory Value Readers ──────────────────────────────────────────────
  double getNodeValue(int index, int property) const;
  double getNodeValue(const std::string &id, int property) const;

  double getNodePressure(const std::string &id) const;
  double getNodeHead(const std::string &id) const;
  double getNodeDemand(const std::string &id) const;
  double getNodeElevation(const std::string &id) const;

  double getLinkValue(int index, int property) const;
  double getLinkValue(const std::string &id, int property) const;

  double getLinkFlow(const std::string &id) const;
  double getLinkVelocity(const std::string &id) const;
  double getLinkHeadloss(const std::string &id) const;
  int getLinkStatus(const std::string &id) const;

  std::vector<NodeResult> getAllNodeResults() const;
  std::vector<LinkResult> getAllLinkResults() const;

  // ── Valve & Link Control (for Isolation Logic) ───────────────────────────
  /**
   * @brief Sets the operational status of a link (EN_OPEN or EN_CLOSED).
   */
  bool setLinkStatus(int index, int status);
  bool setLinkStatus(const std::string &id, int status);

  /**
   * @brief Convenience method to close a pipe or valve (e.g. for burst
   * isolation).
   */
  bool closeLink(const std::string &id);

  /**
   * @brief Convenience method to open a pipe or valve.
   */
  bool openLink(const std::string &id);

  /**
   * @brief Returns true if link is closed.
   */
  bool isLinkClosed(const std::string &id) const;

  // ── Console Reporting & Diagnostics ──────────────────────────────────────
  void printNetworkSummary(std::ostream &os = std::cout) const;
  void printHydraulicResults(std::ostream &os = std::cout) const;

  // ── Error Management ─────────────────────────────────────────────────────
  /**
   * @brief Validates an EPANET return code and prints warnings or errors
   * cleanly.
   * @param errcode EPANET error return code.
   * @param context Context string for informative diagnostics.
   * @return true if success or harmless warning (errcode <= 100), false if
   * fatal error (> 100).
   */
  bool checkError(int errcode, const std::string &context = "") const;

  std::string getErrorMessage(int errcode) const;

  /**
   * @brief Access the underlying C API EN_Project handle if needed.
   */
  EN_Project getProjectHandle() const { return m_ph; }

private:
  EN_Project m_ph;
  bool m_isOpen;
  std::string m_currentInpFile;
};

} // namespace ValveWise

#endif // VALVEWISE_EPANET_ENGINE_HPP
