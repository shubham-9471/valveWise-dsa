cmake_minimum_required(VERSION 3.12)
project(ValveWise VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Global include paths (backend/ and include/ needed everywhere)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backend)

# ==============================================================================
# C Backend (DSA Failure Simulator & Optimizer) — compiled as a static library
# NOTE: epanet_core_runtime.c is NOT included here to avoid duplicate symbols
#       with valvewise_epanet_sim.
# ==============================================================================
set(BACKEND_SOURCES
    backend/graph.c
    backend/hashmap.c
    backend/queue.c
    backend/heap.c
    backend/isolation.c
    backend/optimizer.c
    backend/inp_parser.c
    backend/json_output.c
)

add_library(valvewise_dsa_core STATIC ${BACKEND_SOURCES})
target_include_directories(valvewise_dsa_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)
target_compile_definitions(valvewise_dsa_core PRIVATE epanet_py_EXPORTS)

# Standalone C DSA demo (backend/main.c only — links DSA core, NOT EPANET runtime)
add_executable(valvewise backend/main.c)
target_link_libraries(valvewise PRIVATE valvewise_dsa_core m)

# ==============================================================================
# C++ OWA-EPANET Simulation Engine (links DSA core + EPANET runtime)
# ValveWiseIntegrated.cpp bridges both worlds.
# ==============================================================================
add_executable(valvewise_epanet_sim
    src/main.cpp
    src/EpanetEngine.cpp
    src/epanet_core_runtime.c       # EPANET C runtime — only here, not in dsa_core
    src/ValveWiseIntegrated.cpp     # Integration bridge: C DSA ↔ C++ EpanetEngine
)

target_include_directories(valvewise_epanet_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

# epanet_py_EXPORTS: selects __declspec(dllexport) path in epanet2_2.h on Windows
target_compile_definitions(valvewise_epanet_sim PRIVATE epanet_py_EXPORTS)

# Link DSA core library (provides graph, isolation, optimizer, inp_parser, etc.)
target_link_libraries(valvewise_epanet_sim PRIVATE valvewise_dsa_core)

if(WIN32)
    # Windows: ws2_32 for sockets if needed; m is implicit
else()
    target_link_libraries(valvewise_epanet_sim PRIVATE pthread m)
endif()

# ==============================================================================
# Testing
# ==============================================================================
enable_testing()

add_test(NAME TestEpanetSimImage3
    COMMAND valvewise_epanet_sim ${CMAKE_CURRENT_SOURCE_DIR}/data/net_image3.inp)

add_test(NAME TestEpanetSimSample
    COMMAND valvewise_epanet_sim ${CMAKE_CURRENT_SOURCE_DIR}/data/sample.inp)

add_test(NAME TestBackendIsolation
    COMMAND valvewise --input ${CMAKE_CURRENT_SOURCE_DIR}/data/net_image3.inp --fail P1)

add_test(NAME TestIntegratedPipeline
    COMMAND valvewise_epanet_sim ${CMAKE_CURRENT_SOURCE_DIR}/data/net_image3.inp)
