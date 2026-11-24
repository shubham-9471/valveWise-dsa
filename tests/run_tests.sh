#!/usr/bin/env bash
# tests/run_tests.sh — ValveWise backend integration test suite
# Run from repo root: bash tests/run_tests.sh
# Requires: bash, python3, jq (optional fallback to python3)
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$REPO/backend/valvewise"
DATA="$REPO/data"
TMP="$(mktemp -d)"
PASS=0
FAIL=0
V1_KEYS=(graph reachability isolation structural ranked_links optimization)

die()  { echo "FAIL: $*" >&2; FAIL=$((FAIL+1)); }
pass() { echo "PASS: $*"; PASS=$((PASS+1)); }

# ── Build ──────────────────────────────────────────────────────────────
echo "=== Building backend ==="
make -C "$REPO" all
if [[ ! -x "$BIN" && ! -x "${BIN}.exe" ]]; then
    echo "ERROR: binary not found after build" >&2; exit 1
fi
[[ -x "${BIN}.exe" ]] && BIN="${BIN}.exe"

make -C "$REPO/backend" test-isolation

# ── Helper: assert valid JSON with all v1 keys ─────────────────────────
check_json() {
    local file="$1" label="$2"
    if [[ ! -s "$file" ]]; then
        die "$label: output file missing or empty"; return
    fi
    python3 - "$file" <<'EOF'
import sys, json
with open(sys.argv[1]) as f:
    d = json.load(f)
required = ["graph","reachability","isolation","structural","ranked_links","optimization"]
missing = [k for k in required if k not in d]
if missing:
    sys.exit(f"missing keys: {missing}")
EOF
    if [[ $? -eq 0 ]]; then
        pass "$label: valid JSON with all v1 keys"
    else
        die "$label: JSON validation failed"
    fi
}

# ── Run tests for every .inp file ────────────────────────────────────
for inp in "$DATA"/*.inp; do
    base="$(basename "$inp" .inp)"

    # 1. Basic run — no fail/stuck
    OUT="$TMP/${base}_basic.json"
    if "$BIN" --input "$inp" --output "$OUT"; then
        check_json "$OUT" "$base basic"
    else
        die "$base basic: non-zero exit"
    fi

    # 2. --fail with first pipe found in file (may not exist → warning, exit 0)
    OUT="$TMP/${base}_fail.json"
    if "$BIN" --input "$inp" --fail P1 --budget 2 --output "$OUT"; then
        check_json "$OUT" "$base --fail P1"
    else
        die "$base --fail P1: non-zero exit"
    fi

    # 3. --stuck with first valve id (may not exist → warning, exit 0)
    OUT="$TMP/${base}_stuck.json"
    if "$BIN" --input "$inp" --stuck V1 --budget 2 --output "$OUT"; then
        check_json "$OUT" "$base --stuck V1"
    else
        die "$base --stuck V1: non-zero exit"
    fi

    # 4. --fail + --stuck together
    OUT="$TMP/${base}_both.json"
    if "$BIN" --input "$inp" --fail P1 --stuck V1 --budget 3 --output "$OUT"; then
        check_json "$OUT" "$base --fail+--stuck"
    else
        die "$base --fail+--stuck: non-zero exit"
    fi

    # 5. Missing --input must exit non-zero
    if "$BIN" --output "$TMP/${base}_noinput.json" 2>/dev/null; then
        die "$base: missing --input should be non-zero exit"
    else
        pass "$base: missing --input correctly rejected"
    fi

    # 6. Bad pipe id — must still exit 0 (warning only) and produce valid JSON
    OUT="$TMP/${base}_badpipe.json"
    if "$BIN" --input "$inp" --fail NONEXISTENT_PIPE_XYZ --output "$OUT"; then
        check_json "$OUT" "$base bad pipe id"
    else
        die "$base bad pipe id: non-zero exit"
    fi
done

# ── Summary ────────────────────────────────────────────────────────────
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
rm -rf "$TMP"
[[ $FAIL -eq 0 ]]
