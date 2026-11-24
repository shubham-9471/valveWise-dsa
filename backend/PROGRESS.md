# PROGRESS.md — anti/backend

## Done
- M1: AGENTS.md + PROGRESS.md created
- M2: Fix hashmap.c (<stdint.h>), optimizer.c (_POSIX_C_SOURCE already present),
       main.c (strdup implicit decl + segfault on unknown pipe id),
       inp_parser.c (strcasecmp feature macro)
- M3: .gitattributes LF-endings committed, line endings normalized
- M4: Root Makefile: `make all` = C backend only; `make hydraulic` = C++ part; `make test` added
- M5: tests/run_tests.sh written
- M6: README EPANET-compat claim removed

## Next
- Commit all changes to anti/backend and push

## Decisions
- strdup fix: use portable vw_strdup() helper in main.c guarded by _POSIX_C_SOURCE 200809L
- segfault root cause: failed_link_indices malloc'd for fail_count but written past bounds when pipe not found — fixed with separate fli_count counter (already present, was correct; real crash was strdup returning garbage on MSVC without _strdup alias — added compat shim)
- clock_gettime: _POSIX_C_SOURCE 199309L already in optimizer.c — no change needed
- strcasecmp in inp_parser.c: add #define _POSIX_C_SOURCE 200809L before first #include
- make all: C backend only (backend/valvewise); make hydraulic: C++ valvewise-sim
