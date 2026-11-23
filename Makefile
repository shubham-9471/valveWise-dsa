# ValveWise Top-Level Makefile
# Compiles both the C DSA backend and the C++ EPANET engine, then links them.
CXX = g++
CC  = gcc

# -Iinclude: finds EpanetEngine.hpp, epanet2_2.h, ValveWiseIntegrated.hpp
# -Ibackend: finds graph.h, isolation.h, optimizer.h, etc.
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -Iinclude -Ibackend -Depanet_py_EXPORTS
CFLAGS   = -std=c11  -Wall -Wextra -O2 -g -Iinclude -Ibackend -Depanet_py_EXPORTS

ifeq ($(OS),Windows_NT)
    TARGET_SIM = valvewise-sim.exe
    CLEAN_CMD  = cmd /c "del /q src\*.o backend\*.o $(TARGET_SIM) 2>nul"
else
    TARGET_SIM = valvewise-sim
    CLEAN_CMD  = rm -f src/*.o backend/*.o $(TARGET_SIM)
endif

# ── C backend object files ────────────────────────────────────────────────────
BACKEND_OBJS = \
    backend/graph.o      \
    backend/hashmap.o    \
    backend/queue.o      \
    backend/heap.o       \
    backend/isolation.o  \
    backend/optimizer.o  \
    backend/inp_parser.o \
    backend/json_output.o

# ── C++ source object files ───────────────────────────────────────────────────
SIM_OBJS = \
    src/main.o                  \
    src/EpanetEngine.o          \
    src/epanet_core_runtime.o   \
    src/ValveWiseIntegrated.o

.PHONY: all clean test test-image3 test-sample backend

all: $(TARGET_SIM) backend

# Backend standalone (C only)
backend:
	$(MAKE) -C backend

# ── Link final executable ─────────────────────────────────────────────────────
$(TARGET_SIM): $(SIM_OBJS) $(BACKEND_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lm
	@echo "Build successful: $(TARGET_SIM)"

# ── C++ compile rules ─────────────────────────────────────────────────────────
src/main.o: src/main.cpp include/EpanetEngine.hpp include/epanet2_2.h include/epanet2_enums.h include/ValveWiseIntegrated.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/EpanetEngine.o: src/EpanetEngine.cpp include/EpanetEngine.hpp include/epanet2_2.h include/epanet2_enums.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/ValveWiseIntegrated.o: src/ValveWiseIntegrated.cpp include/ValveWiseIntegrated.hpp include/EpanetEngine.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# ── C compile rules ───────────────────────────────────────────────────────────
src/epanet_core_runtime.o: src/epanet_core_runtime.c include/epanet2_2.h include/epanet2_enums.h
	$(CC) $(CFLAGS) -c -o $@ $<

backend/%.o: backend/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Test targets ──────────────────────────────────────────────────────────────
test: test-image3

test-image3: $(TARGET_SIM)
	./$(TARGET_SIM) data/net_image3.inp

test-sample: $(TARGET_SIM)
	./$(TARGET_SIM) data/sample.inp

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	$(MAKE) -C backend clean
	$(CLEAN_CMD)
