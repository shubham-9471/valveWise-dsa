# ValveWise — Root Makefile
# `make all`      → C backend only  (backend/valvewise)
# `make hydraulic` → C++ approximate hydraulic engine (valvewise-sim)
# `make test`     → build backend then run tests/run_tests.sh
# `make clean`    → remove all build artefacts

CXX = g++
CC  = gcc

CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -Iinclude -Ibackend -Depanet_py_EXPORTS
CFLAGS   = -std=c11  -Wall -Wextra -O2 -g -Iinclude -Ibackend

ifeq ($(OS),Windows_NT)
    HYD_TARGET = valvewise-sim.exe
    CLEAN_CMD  = cmd /c "del /q src\*.o backend\*.o $(HYD_TARGET) 2>nul"
else
    HYD_TARGET = valvewise-sim
    CLEAN_CMD  = rm -f src/*.o backend/*.o $(HYD_TARGET)
endif

# ── C++ hydraulic object files ────────────────────────────────────────
HYD_OBJS = \
    src/main.o                \
    src/EpanetEngine.o        \
    src/epanet_core_runtime.o \
    src/ValveWiseIntegrated.o

.PHONY: all backend hydraulic test clean

# Default: C backend only
all: backend

# Backend standalone (C only) — delegates to backend/Makefile
backend:
	$(MAKE) -C backend

# Optional C++ hydraulic part
hydraulic: $(HYD_TARGET)

$(HYD_TARGET): $(HYD_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lm
	@echo "Hydraulic build: $(HYD_TARGET)"

src/main.o: src/main.cpp include/EpanetEngine.hpp include/epanet2_2.h include/epanet2_enums.h include/ValveWiseIntegrated.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/EpanetEngine.o: src/EpanetEngine.cpp include/EpanetEngine.hpp include/epanet2_2.h include/epanet2_enums.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/ValveWiseIntegrated.o: src/ValveWiseIntegrated.cpp include/ValveWiseIntegrated.hpp include/EpanetEngine.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/epanet_core_runtime.o: src/epanet_core_runtime.c include/epanet2_2.h include/epanet2_enums.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Run tests: build backend then execute the test script
test: backend
	bash tests/run_tests.sh

clean:
	$(MAKE) -C backend clean
	$(CLEAN_CMD)
