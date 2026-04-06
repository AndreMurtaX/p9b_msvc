#!/usr/bin/env bash
EXE="${1:-build_vs18/cli/Debug/p9b.exe}"
TOTAL_PASS=0
TOTAL_FAIL=0

run_suite() {
    local result
    result=$(bash "$1" "$EXE" 2>&1)
    echo "$result"
    local p f
    p=$(echo "$result" | grep -oP '^\d+(?= passed)' | tail -1)
    f=$(echo "$result" | grep -oP '^\d+(?= failed)' | tail -1)
    TOTAL_PASS=$((TOTAL_PASS + ${p:-0}))
    TOTAL_FAIL=$((TOTAL_FAIL + ${f:-0}))
}

run_suite tests/review/run_tests.sh
run_suite tests/phase2/run_tests.sh
run_suite tests/phase3/run_tests.sh
run_suite tests/phase4/run_tests.sh
run_suite tests/phase5/run_tests.sh
run_suite tests/phase6/run_tests.sh
run_suite tests/phase7/run_tests.sh
run_suite tests/phase8/run_tests.sh
run_suite tests/phase9/run_tests.sh

echo ""
echo "========================================"
echo "TOTAL: $TOTAL_PASS passed, $TOTAL_FAIL failed"
echo "========================================"
