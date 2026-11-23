cmake_minimum_required(VERSION 3.12)
project(ValveWise VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/backend)

# ==============================================================================
# C Backend (DSA Failure Simulator & Optimizer)
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

add_executable(valvewise backend/main.c)
target_link_libraries(valvewise PRIVATE valvewise_dsa_core m)

# ==============================================================================
# C++ OWA-EPANET Simulation Engine
# ==============================================================================
add_executable(valvewise_epanet_sim
    src/main.cpp
    src/EpanetEngine.cpp
    src/epanet_core_runtime.c
)

target_include_directories(valvewise_epanet_sim PRIVATE include)
target_compile_definitions(valvewise_epanet_sim PRIVATE epanet_py_EXPORTS)

if(WIN32)
    # Link math or sockets if needed
else()
    target_link_libraries(valvewise_epanet_sim PRIVATE pthread m)
endif()

# ==============================================================================
# Testing target
# ==============================================================================
enable_testing()
add_test(NAME TestEpanetSimImage3
    COMMAND valvewise_epanet_sim ${CMAKE_CURRENT_SOURCE_DIR}/data/net_image3.inp)
add_test(NAME TestEpanetSimSample
    COMMAND valvewise_epanet_sim ${CMAKE_CURRENT_SOURCE_DIR}/data/sample.inp)
add_test(NAME TestBackendIsolation
    COMMAND valvewise --input ${CMAKE_CURRENT_SOURCE_DIR}/data/net_image3.inp --fail P1)
