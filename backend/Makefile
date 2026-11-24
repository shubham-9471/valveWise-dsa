# ValveWise Backend Makefile
# Builds the valvewise CLI binary from all C source files.
#
# Linux/macOS:   make
# Windows (MSYS2/MinGW): mingw32-make
#
# Prerequisites (Windows):
#   1. Install MSYS2: https://www.msys2.org/
#   2. In MSYS2 terminal: pacman -S mingw-w64-x86_64-gcc make
#   3. Add C:\msys64\mingw64\bin to your Windows PATH
#   4. Run: mingw32-make  (from the backend/ directory)

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2 -g -pedantic
LDFLAGS = -lm

TARGET  = valvewise

SRCS = graph.c \
       hashmap.c \
       queue.c \
       heap.c \
       isolation.c \
       optimizer.c \
       inp_parser.c \
       json_output.c \
       main.c

OBJS = $(SRCS:.c=.o)

# ── Default target ───────────────────────────────────────────────────
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

# ── Object file rules ────────────────────────────────────────────────
graph.o:       graph.c graph.h
	$(CC) $(CFLAGS) -c -o $@ $<

hashmap.o:     hashmap.c hashmap.h
	$(CC) $(CFLAGS) -c -o $@ $<

queue.o:       queue.c queue.h
	$(CC) $(CFLAGS) -c -o $@ $<

heap.o:        heap.c heap.h
	$(CC) $(CFLAGS) -c -o $@ $<

isolation.o:   isolation.c isolation.h graph.h queue.h
	$(CC) $(CFLAGS) -c -o $@ $<

optimizer.o:   optimizer.c optimizer.h graph.h heap.h isolation.h
	$(CC) $(CFLAGS) -c -o $@ $<

inp_parser.o:  inp_parser.c inp_parser.h graph.h hashmap.h
	$(CC) $(CFLAGS) -c -o $@ $<

json_output.o: json_output.c json_output.h graph.h isolation.h optimizer.h
	$(CC) $(CFLAGS) -c -o $@ $<

main.o:        main.c graph.h hashmap.h inp_parser.h isolation.h optimizer.h json_output.h
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Run with sample network ──────────────────────────────────────────
run: $(TARGET)
	./$(TARGET) --input ../data/sample.inp --output output.json --verbose

# ── Test: simulate a pipe failure ───────────────────────────────────
test: $(TARGET)
	./$(TARGET) --input ../data/sample.inp --fail P5 --budget 3 \
	            --output test_output.json --verbose
	@echo "Test output written to test_output.json"

# ── Test: multiple failures + stuck valve ───────────────────────────
test-multi: $(TARGET)
	./$(TARGET) --input ../data/sample.inp --fail P3,P7 --stuck V1 \
	            --budget 5 --output test_multi.json --verbose

test-isolation: graph.o queue.o isolation.o ../tests/isolation_model_test.c
	$(CC) $(CFLAGS) -I. -o ../tests/isolation_model_test.exe ../tests/isolation_model_test.c graph.o queue.o isolation.o
	../tests/isolation_model_test.exe

# ── Clean ────────────────────────────────────────────────────────────
clean:
ifeq ($(OS),Windows_NT)
	-cmd /c del /q $(OBJS) $(TARGET) $(TARGET).exe output.json test_output.json test_multi.json ..\tests\isolation_model_test.exe 2>nul
else
	rm -f $(OBJS) $(TARGET) $(TARGET).exe output.json test_output.json test_multi.json ../tests/isolation_model_test.exe
endif

.PHONY: all run test test-multi test-isolation clean
