# ValveWise Backend — Agent Rules (anti/backend)

## Ownership
Backend engineer owns ONLY: backend/** src/** include/** data/** tests/** bench/** tools/**
Makefile CMakeLists.txt README.md .gitignore .gitattributes docs/CONTRACT_CHANGES.md
NEVER touch frontend/** or server/**

## Frozen Interfaces (additive changes only)
- CLI binary: backend/valvewise (.exe on Windows)
  Flags: --input F --fail P1,P2 --stuck V1 --budget N --output out.json [--verbose]
  On failure: non-zero exit + {"error":"..."} to --output if given
- Output JSON top-level keys: graph reachability isolation structural ranked_links optimization
  Never rename/remove/retype existing fields. Log every addition in docs/CONTRACT_CHANGES.md.

## Git
- Branch: anti/backend
- One commit per milestone
- Push to origin anti/backend if remote exists
- Never touch main or other branches

## Token Rules
- grep/head/sed ranges before reading; never re-read unchanged files
- Never dump whole files or large JSON; extract keys with python
- Patch functions surgically; rewrite only new files
- Trim command output (tail -20); batch commands
- No re-explaining plans; one status line per milestone
- Record decisions in backend/PROGRESS.md (max 15 lines); continue without asking
- Stop only on true blocker; report in under 10 lines
