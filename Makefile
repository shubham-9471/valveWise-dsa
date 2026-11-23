# ValveWise Top-Level Makefile
CXX = g++
CC  = gcc
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude -Depanet_py_EXPORTS
CFLAGS   = -std=c11 -Wall -Wextra -O2 -Iinclude -Depanet_py_EXPORTS

ifeq ($(OS),Windows_NT)
    TARGET_SIM = valvewise-sim.exe
    CLEAN_CMD  = cmd /c "del /q src\*.o $(TARGET_SIM) 2>nul"
else
    TARGET_SIM = valvewise-sim
    CLEAN_CMD  = rm -f src/*.o $(TARGET_SIM)
endif

.PHONY: all clean test test-image3 test-sample backend

all: $(TARGET_SIM) backend

backend:
	$(MAKE) -C backend

$(TARGET_SIM): src/main.o src/EpanetEngine.o src/epanet_core_runtime.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lm
	@echo "Build successful: $(TARGET_SIM)"

src/main.o: src/main.cpp include/EpanetEngine.hpp include/epanet2_2.h include/epanet2_enums.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/EpanetEngine.o: src/EpanetEngine.cpp include/EpanetEngine.hpp include/epanet2_2.h include/epanet2_enums.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/epanet_core_runtime.o: src/epanet_core_runtime.c include/epanet2_2.h include/epanet2_enums.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: test-image3

test-image3: $(TARGET_SIM)
	./$(TARGET_SIM) data/net_image3.inp

test-sample: $(TARGET_SIM)
	./$(TARGET_SIM) data/sample.inp

clean:
	$(MAKE) -C backend clean
	$(CLEAN_CMD)
