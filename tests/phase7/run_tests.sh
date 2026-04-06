#!/usr/bin/env bash
# Run all Phase 7 tests.
# Usage: bash tests/phase7/run_tests.sh [path/to/p9b.exe]

EXE="${1:-build_vs18/cli/Debug/p9b.exe}"
DIR="tests/phase7"
PASS=0; FAIL=0

ROOT_ABS="$(pwd)"
EXE_ABS="$ROOT_ABS/$EXE"

run_test() {
    local name="$1" desc="$2" expected="$3" expect_error="${4:-}" stdin_data="${5:-}"
    local bas_file="$ROOT_ABS/$DIR/$name"
    local actual
    if [ -n "$stdin_data" ]; then
        actual=$(echo "$stdin_data" | timeout 10 "$EXE_ABS" "$bas_file" 2>&1 | tr -d '\r')
    else
        actual=$(timeout 10 "$EXE_ABS" "$bas_file" 2>&1 | tr -d '\r')
    fi

    if [ -n "$expect_error" ]; then
        if echo "$actual" | grep -qi "$expect_error"; then
            echo "[PASS] $name  $desc"
            PASS=$((PASS+1))
        else
            echo "[FAIL] $name  $desc  expected_pattern='$expect_error'  got='$actual'"
            FAIL=$((FAIL+1))
        fi
    else
        if [ "$actual" = "$expected" ]; then
            echo "[PASS] $name  $desc"
            PASS=$((PASS+1))
        else
            echo "[FAIL] $name  $desc"
            echo "  expected: $(echo "$expected" | head -5)"
            echo "  got:      $(echo "$actual"   | head -5)"
            FAIL=$((FAIL+1))
        fi
    fi
}

echo "=== Phase 7: INCLUDE ==="

run_test t01_include.bas  "INCLUDE expands helper.bas"  "$(printf '21\n30')"

echo ""
echo "$PASS passed, $FAIL failed"
