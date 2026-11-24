# ValveWise — Water Distribution Network Failure & Resilience Optimizer

> **Scope notice:** ValveWise is a **connectivity simulator**, not a full hydraulic solver.
> It reads EPANET-format `.inp` files for network topology only.
> The `src/` C++ engine is a **hand-written approximate hydraulic stand-in** — it is *not*
> OWA-EPANET and makes no claim of EPANET compatibility. For regulatory or engineering
> hydraulic analysis, use a certified EPANET solver.

A modular C-based simulation and optimization engine for Water Distribution Networks (WDNs). ValveWise
parses EPANET-style `.inp` network topologies, models pipe burst failures and valve operability issues
(including stuck-open valves), evaluates reachability and disrupted demand, detects topological
vulnerabilities, and optimizes strategic valve placement under budget constraints.

---

## 🏗️ Project Architecture

```
ValveWise/
├── backend/
│   ├── graph.h / graph.c         # Adjacency list representation & network graph ADT
│   ├── hashmap.h / hashmap.c     # Open-addressing hash table (djb2) mapping IDs to indices
│   ├── queue.h / queue.c         # Circular-buffer integer queue for BFS traversals
│   ├── heap.h / heap.c           # Binary Min/Max-Heap for component criticality ranking
│   ├── isolation.h / isolation.c # BFS reachability, Tarjan bridge detection, isolation boundaries
│   ├── optimizer.h / optimizer.c # Greedy, Random, Topology, and Exhaustive valve placement
│   ├── inp_parser.h / inp_parser.c # EPANET-style .inp network file parser
│   ├── json_output.h / json_output.c # Dynamic string buffer JSON serializer
│   ├── main.c                    # C CLI entrypoint and argument parsing
│   └── Makefile                  # Backend C11 compilation targets
├── include/
│   ├── epanet2_2.h               # OWA-EPANET 2.2+ thread-safe C header
│   ├── epanet2_enums.h           # OWA-EPANET property & object enumerations
│   └── EpanetEngine.hpp          # Modern C++ RAII wrapper class for OWA-EPANET
├── src/
│   ├── EpanetEngine.cpp          # In-memory hydraulic simulation & valve control methods
│   ├── epanet_core_runtime.c     # Approximate hydraulic stand-in (hand-written, not OWA-EPANET)
│   └── main.cpp                  # C++ test runner & demonstration executable
├── docs/
│   ├── Doxyfile                  # Doxygen configuration file
│   ├── DoxygenLayout.xml         # Modernized Doxygen layout specification
│   └── doxygen-awesome.css       # Clean stylesheet for documentation pages
├── data/
│   ├── sample.inp                # Sample WDN (reservoirs, tank, junctions, pipes, valves)
│   └── net_image3.inp            # Topologically verified test network (diagram image 3)
├── CMakeLists.txt                # Cross-platform CMake configuration for C & C++
├── Makefile                      # Top-level root Makefile
├── .gitignore                    # Ignores object files, binaries, and test outputs
└── README.md                     # Documentation and usage guide
```

---

## 📊 Data Structures Used

| Structure | Implementation | File | Complexity | Purpose in ValveWise |
|---|---|---|---|---|
| **Hash Table** | Open addressing, linear probing, tombstones, djb2 | `hashmap.h/c` | $O(1)$ avg lookup / insert | Maps string IDs (`J1`, `P5`, `V1`) to dense integer indices |
| **Adjacency List** | Dynamic array of node heads + singly linked `Edge` nodes | `graph.h/c` | $O(V + E)$ storage | Represents the water distribution graph with edge metadata |
| **Circular Queue** | Ring buffer with `head`, `tail`, `size`, `capacity` | `queue.h/c` | $O(1)$ enqueue / dequeue | Efficient BFS for water source reachability and valve tracing |
| **Binary Heap** | Array-based min/max heap with sift-up and sift-down | `heap.h/c` | $O(\log N)$ push / pop | Priority ranking of critical pipes and high-degree candidates |
| **DFS Call Stack & Arrays** | Tarjan's discovery (`disc`), low-link (`low`), `parent` | `isolation.c` | $O(V + E)$ traversal | Identification of bridges and articulation points |

---

## ⚙️ Algorithms Implemented

1. **BFS Reachability Analysis**:
   - Traverses network starting from active water supply sources (reservoirs and tanks).
   - Skips burst pipes and closed valves to compute reachable vs. disconnected junctions and total disconnected consumer demand ($L/s$).

2. **Isolation Boundary Determination & Stuck Valve Handling**:
   - Starting from a burst pipe endpoint, explores outward to identify the nearest operable isolation valves to shut.
   - Accurately models **stuck-open valves**: when a valve fails to close, the algorithm expands the isolation boundary further upstream/downstream and records additional disrupted customer demand.
   - Preserves underlying pipe and valve statuses without altering permanent graph state.

3. **Tarjan's Bridge & Articulation Point Detection (DFS)**:
   - Uses low-link values to detect structural single points of failure (bridges whose failure separates the network).

4. **Budget-Constrained Valve Placement Optimization**:
   - **Greedy Placement**: Evaluates marginal disruption reduction across scenarios and greedily selects links up to budget $B$.
   - **Random Baseline**: Selects $B$ random candidates across uniform iterations for benchmarking.
   - **Topology-Based Baseline**: Prioritizes bridges first, then links with maximal endpoint degree products.
   - **Exhaustive Search**: Enumerates all $\binom{E}{B}$ combinations on small networks (skips automatically when candidate space $> 500,000$).

---

## 👥 2-Person Team Division

- **Teammate A (Core Graph ADT & Parsers)**:
  - `graph.h / graph.c`, `hashmap.h / hashmap.c`, `queue.h / queue.c`, `heap.h / heap.c`, `inp_parser.h / inp_parser.c`, `data/sample.inp`.
- **Teammate B (Simulation Engine, Optimization & CLI/Output)**:
  - `isolation.h / isolation.c`, `optimizer.h / optimizer.c`, `json_output.h / json_output.c`, `main.c`, `Makefile`.

---

## 🛠️ How to Compile & Run

### Prerequisites
- GCC / G++ supporting C11 and C++17 (Linux/macOS or MinGW-w64 on Windows).
- CMake 3.12+ (optional for CMake-based builds).
- Doxygen 1.9+ (optional for generating HTML documentation).

### Quick Build & Run (Root Makefile)
Run from the root `ValveWise/` directory:
```bash
# Build C++ simulation engine and C backend
make all

# Run test on the Image 3 topology network (R1 -> PUMP -> J1 with T1, J2, J3)
make test-image3

# Run test on the sample 11-node network
make test-sample
```
*On Windows with MinGW:*
```powershell
mingw32-make all
mingw32-make test-image3
```

### Direct C++ Simulation CLI Usage
```bash
./valvewise-sim data/net_image3.inp
```
Output features:
- In-memory execution (empty report & binary output paths `""` to prevent writing `.rpt` files to disk).
- Instantaneous hydraulic solver (`EN_solveH`) reporting heads, pressures, demands, flows, and velocities.
- Step-by-step extended-period time loop (`EN_runH` / `EN_nextH`).
- Dynamic valve/pipe closure demonstration (`closeLink("P1")`) verifying flow drops to 0 and reroutes.
- Clean error trapping for `errcode > 100` and automatic RAII cleanup (`EN_deleteproject`).

### C Backend Build & CLI Usage
Navigate to the `backend/` directory:
```bash
cd backend
make clean && make
```
*On Windows with MinGW:*
```powershell
cd backend
mingw32-make clean
mingw32-make
```

Running backend simulation scenarios:
1. **Default Full Run (Runs structural analysis, ranking, and all 4 optimization baselines)**:
   ```bash
   ./valvewise --input ../data/sample.inp --output output.json --verbose
   ```
2. **Single Pipe Failure Isolation**:
   ```bash
   ./valvewise --input ../data/sample.inp --fail P5 --budget 3 --output test_output.json --verbose
   ```
3. **Multi-Failure with Stuck Valve**:
   ```bash
   ./valvewise --input ../data/sample.inp --fail P3,P7 --stuck V1 --budget 5 --output test_multi.json --verbose
   ```
4. **Test on Image 3 Topology**:
   ```bash
   ./valvewise --input ../data/net_image3.inp --fail P1 --verbose
   ```

### 📚 Generating Doxygen Documentation
```bash
cd docs
doxygen Doxyfile
```
The modern HTML documentation styled with `doxygen-awesome.css` will be generated in `docs/html/html/index.html`.

---

## 📄 Output JSON Format

The output file (e.g. `output.json`) contains:
- `graph`: List of nodes and links with lengths, diameters, status, coordinates, and demands.
- `reachability`: Disconnected junction count, disconnected demand, and disconnected node IDs.
- `isolation`: Failed links, valves to close, nodes extra disrupted by stuck valves, and total disrupted demand.
- `structural`: Connected components count, list of bridge link IDs, and articulation point node IDs.
- `ranked_links`: Links ranked by disruption severity along with human-readable explanations.
- `optimization`: Comparative benchmark containing `greedy`, `random`, `topology`, and `exhaustive` performance (runtime in ms, candidates evaluated, and percentage improvement over baseline).
