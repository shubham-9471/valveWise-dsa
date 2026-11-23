/**
 * @file EpanetEngine.cpp
 * @brief Implementation of EpanetEngine C++ wrapper for OWA-EPANET 2.2+.
 */

#include "EpanetEngine.hpp"
#include <iomanip>
#include <sstream>

namespace ValveWise {

EpanetEngine::EpanetEngine(const std::string &inpFilePath)
    : m_ph(nullptr), m_isOpen(false), m_currentInpFile("") {
  int err = EN_createproject(&m_ph);
  if (!checkError(err, "EN_createproject")) {
    throw std::runtime_error("Failed to initialize EPANET project handle: " +
                             getErrorMessage(err));
  }

  if (!inpFilePath.empty()) {
    open(inpFilePath, "", "");
  }
}

EpanetEngine::~EpanetEngine() {
  try {
    if (m_isOpen) {
      close();
    }
    if (m_ph) {
      EN_deleteproject(m_ph);
      m_ph = nullptr;
    }
  } catch (...) {
    // Suppress exceptions in destructor
  }
}

EpanetEngine::EpanetEngine(EpanetEngine &&other) noexcept
    : m_ph(other.m_ph), m_isOpen(other.m_isOpen),
      m_currentInpFile(std::move(other.m_currentInpFile)) {
  other.m_ph = nullptr;
  other.m_isOpen = false;
}

EpanetEngine &EpanetEngine::operator=(EpanetEngine &&other) noexcept {
  if (this != &other) {
    if (m_isOpen) {
      close();
    }
    if (m_ph) {
      EN_deleteproject(m_ph);
    }
    m_ph = other.m_ph;
    m_isOpen = other.m_isOpen;
    m_currentInpFile = std::move(other.m_currentInpFile);

    other.m_ph = nullptr;
    other.m_isOpen = false;
  }
  return *this;
}

bool EpanetEngine::open(const std::string &inpFile, const std::string &rptFile,
                        const std::string &outFile) {
  if (m_isOpen) {
    close();
  }

  if (!m_ph) {
    int err = EN_createproject(&m_ph);
    if (!checkError(err, "EN_createproject"))
      return false;
  }

  // Pass "" for rptFile and outFile for 100% in-memory simulation without
  // generating disk files
  int err = EN_open(m_ph, inpFile.c_str(), rptFile.c_str(), outFile.c_str());
  if (!checkError(err, "EN_open: " + inpFile)) {
    return false;
  }

  m_isOpen = true;
  m_currentInpFile = inpFile;
  return true;
}

bool EpanetEngine::close() {
  if (!m_isOpen || !m_ph)
    return true;
  int err = EN_close(m_ph);
  m_isOpen = false;
  m_currentInpFile.clear();
  return checkError(err, "EN_close");
}

bool EpanetEngine::solveH() {
  if (!m_isOpen) {
    std::cerr
        << "[EpanetEngine] Error: Cannot run solveH on an unopened project.\n";
    return false;
  }
  int err = EN_solveH(m_ph);
  return checkError(err, "EN_solveH");
}

bool EpanetEngine::runHydraulicsStepByStep(
    std::function<void(long currentTime, long tstep)> onStep) {
  if (!m_isOpen) {
    std::cerr << "[EpanetEngine] Error: Cannot run hydraulics on an unopened "
                 "project.\n";
    return false;
  }

  int err = EN_openH(m_ph);
  if (!checkError(err, "EN_openH"))
    return false;

  // EN_NOSAVE avoids intermediate hydraulic save files
  err = EN_initH(m_ph, EN_NOSAVE);
  if (!checkError(err, "EN_initH")) {
    EN_closeH(m_ph);
    return false;
  }

  long t = 0;
  long tstep = 0;
  do {
    err = EN_runH(m_ph, &t);
    if (!checkError(err, "EN_runH")) {
      EN_closeH(m_ph);
      return false;
    }

    err = EN_nextH(m_ph, &tstep);
    if (!checkError(err, "EN_nextH")) {
      EN_closeH(m_ph);
      return false;
    }

    if (onStep) {
      onStep(t, tstep);
    }
  } while (tstep > 0);

  err = EN_closeH(m_ph);
  return checkError(err, "EN_closeH");
}

bool EpanetEngine::solveQ() {
  if (!m_isOpen) {
    std::cerr
        << "[EpanetEngine] Error: Cannot run solveQ on an unopened project.\n";
    return false;
  }
  int err = EN_solveQ(m_ph);
  return checkError(err, "EN_solveQ");
}

int EpanetEngine::getNodeCount() const {
  if (!m_isOpen)
    return 0;
  int count = 0;
  EN_getcount(m_ph, EN_NODECOUNT, &count);
  return count;
}

int EpanetEngine::getLinkCount() const {
  if (!m_isOpen)
    return 0;
  int count = 0;
  EN_getcount(m_ph, EN_LINKCOUNT, &count);
  return count;
}

std::string EpanetEngine::getNodeId(int index) const {
  if (!m_isOpen)
    return "";
  char id[256] = {0};
  int err = EN_getnodeid(m_ph, index, id);
  if (err > 100)
    return "";
  return std::string(id);
}

int EpanetEngine::getNodeIndex(const std::string &id) const {
  if (!m_isOpen)
    return 0;
  int idx = 0;
  int err = EN_getnodeindex(m_ph, const_cast<char *>(id.c_str()), &idx);
  if (err > 100)
    return 0;
  return idx;
}

std::string EpanetEngine::getLinkId(int index) const {
  if (!m_isOpen)
    return "";
  char id[256] = {0};
  int err = EN_getlinkid(m_ph, index, id);
  if (err > 100)
    return "";
  return std::string(id);
}

int EpanetEngine::getLinkIndex(const std::string &id) const {
  if (!m_isOpen)
    return 0;
  int idx = 0;
  int err = EN_getlinkindex(m_ph, const_cast<char *>(id.c_str()), &idx);
  if (err > 100)
    return 0;
  return idx;
}

double EpanetEngine::getNodeValue(int index, int property) const {
  if (!m_isOpen)
    return 0.0;
  double val = 0.0;
  EN_getnodevalue(m_ph, index, property, &val);
  return val;
}

double EpanetEngine::getNodeValue(const std::string &id, int property) const {
  int idx = getNodeIndex(id);
  if (idx <= 0)
    return 0.0;
  return getNodeValue(idx, property);
}

double EpanetEngine::getNodePressure(const std::string &id) const {
  return getNodeValue(id, EN_PRESSURE);
}

double EpanetEngine::getNodeHead(const std::string &id) const {
  return getNodeValue(id, EN_HEAD);
}

double EpanetEngine::getNodeDemand(const std::string &id) const {
  return getNodeValue(id, EN_DEMAND);
}

double EpanetEngine::getNodeElevation(const std::string &id) const {
  return getNodeValue(id, EN_ELEVATION);
}

double EpanetEngine::getLinkValue(int index, int property) const {
  if (!m_isOpen)
    return 0.0;
  double val = 0.0;
  EN_getlinkvalue(m_ph, index, property, &val);
  return val;
}

double EpanetEngine::getLinkValue(const std::string &id, int property) const {
  int idx = getLinkIndex(id);
  if (idx <= 0)
    return 0.0;
  return getLinkValue(idx, property);
}

double EpanetEngine::getLinkFlow(const std::string &id) const {
  return getLinkValue(id, EN_FLOW);
}

double EpanetEngine::getLinkVelocity(const std::string &id) const {
  return getLinkValue(id, EN_VELOCITY);
}

double EpanetEngine::getLinkHeadloss(const std::string &id) const {
  return getLinkValue(id, EN_HEADLOSS);
}

int EpanetEngine::getLinkStatus(const std::string &id) const {
  return static_cast<int>(getLinkValue(id, EN_STATUS));
}

std::vector<NodeResult> EpanetEngine::getAllNodeResults() const {
  std::vector<NodeResult> results;
  int n = getNodeCount();
  results.reserve(n);
  for (int i = 1; i <= n; ++i) {
    NodeResult nr;
    nr.index = i;
    nr.id = getNodeId(i);
    nr.elevation = getNodeValue(i, EN_ELEVATION);
    nr.demand = getNodeValue(i, EN_DEMAND);
    nr.head = getNodeValue(i, EN_HEAD);
    nr.pressure = getNodeValue(i, EN_PRESSURE);
    results.push_back(nr);
  }
  return results;
}

std::vector<LinkResult> EpanetEngine::getAllLinkResults() const {
  std::vector<LinkResult> results;
  int m = getLinkCount();
  results.reserve(m);
  for (int i = 1; i <= m; ++i) {
    LinkResult lr;
    lr.index = i;
    lr.id = getLinkId(i);
    { int lt = 0; EN_getlinktype(m_ph, i, &lt); lr.linkType = lt; }
    lr.status = static_cast<int>(getLinkValue(i, EN_STATUS));
    lr.flow = getLinkValue(i, EN_FLOW);
    lr.velocity = getLinkValue(i, EN_VELOCITY);
    lr.headloss = getLinkValue(i, EN_HEADLOSS);
    results.push_back(lr);
  }
  return results;
}

bool EpanetEngine::setLinkStatus(int index, int status) {
  if (!m_isOpen)
    return false;
  int err =
      EN_setlinkvalue(m_ph, index, EN_STATUS, static_cast<double>(status));
  return checkError(err,
                    "EN_setlinkvalue (index " + std::to_string(index) + ")");
}

bool EpanetEngine::setLinkStatus(const std::string &id, int status) {
  int idx = getLinkIndex(id);
  if (idx <= 0) {
    std::cerr << "[EpanetEngine] Link ID '" << id << "' not found.\n";
    return false;
  }
  return setLinkStatus(idx, status);
}

bool EpanetEngine::closeLink(const std::string &id) {
  return setLinkStatus(id, EN_CLOSED);
}

bool EpanetEngine::openLink(const std::string &id) {
  return setLinkStatus(id, EN_OPEN);
}

bool EpanetEngine::isLinkClosed(const std::string &id) const {
  return getLinkStatus(id) == EN_CLOSED;
}

void EpanetEngine::printNetworkSummary(std::ostream &os) const {
  os << "\n======================================================\n";
  os << "           ValveWise EPANET Network Summary           \n";
  os << "======================================================\n";
  os << " Input File:    "
     << (m_currentInpFile.empty() ? "(none)" : m_currentInpFile) << "\n";
  os << " Total Nodes:   " << getNodeCount() << "\n";
  os << " Total Links:   " << getLinkCount() << "\n";
  os << " In-Memory Run: Yes (Report & Binary file generation disabled)\n";
  os << "------------------------------------------------------\n";
}

void EpanetEngine::printHydraulicResults(std::ostream &os) const {
  os << "\n--- NODE HYDRAULIC RESULTS ---\n";
  os << std::left << std::setw(8) << "Index" << std::setw(12) << "Node ID"
     << std::setw(12) << "Elev(ft)" << std::setw(14) << "Demand(gpm)"
     << std::setw(12) << "Head(ft)" << std::setw(14) << "Pressure(psi)" << "\n";
  os << std::string(72, '-') << "\n";

  auto nodes = getAllNodeResults();
  for (const auto &n : nodes) {
    os << std::left << std::setw(8) << n.index << std::setw(12) << n.id
       << std::fixed << std::setprecision(2) << std::setw(12) << n.elevation
       << std::setw(14) << n.demand << std::setw(12) << n.head << std::setw(14)
       << n.pressure << "\n";
  }

  os << "\n--- LINK HYDRAULIC RESULTS ---\n";
  os << std::left << std::setw(8) << "Index" << std::setw(12) << "Link ID"
     << std::setw(10) << "Status" << std::setw(14) << "Flow(gpm)"
     << std::setw(14) << "Velocity(fps)" << std::setw(14) << "Headloss(/kft)"
     << "\n";
  os << std::string(72, '-') << "\n";

  auto links = getAllLinkResults();
  for (const auto &l : links) {
    std::string statusStr = (l.status == EN_CLOSED) ? "CLOSED" : "OPEN";
    os << std::left << std::setw(8) << l.index << std::setw(12) << l.id
       << std::setw(10) << statusStr << std::fixed << std::setprecision(2)
       << std::setw(14) << l.flow << std::setw(14) << l.velocity
       << std::setw(14) << l.headloss << "\n";
  }
  os << "======================================================\n\n";
}

bool EpanetEngine::checkError(int errcode, const std::string &context) const {
  if (errcode == 0)
    return true;

  if (errcode <= 100) {
    std::cerr << "[EpanetEngine WARNING " << errcode << "] "
              << (context.empty() ? "" : "(" + context + ") ")
              << getErrorMessage(errcode) << "\n";
    return true; // harmless warning
  } else {
    std::cerr << "[EpanetEngine ERROR " << errcode << "] "
              << (context.empty() ? "" : "(" + context + ") ")
              << getErrorMessage(errcode) << "\n";
    return false; // fatal error
  }
}

std::string EpanetEngine::getErrorMessage(int errcode) const {
  if (errcode == 0)
    return "No error";
  char errmsg[512] = {0};
  EN_geterror(errcode, errmsg, sizeof(errmsg) - 1);
  if (errmsg[0] == '\0') {
    std::ostringstream ss;
    ss << "EPANET error code " << errcode;
    return ss.str();
  }
  return std::string(errmsg);
}

} // namespace ValveWise
