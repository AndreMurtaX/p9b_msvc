#!/usr/bin/env bash
# Run all review / regression-fix tests.
# Usage: bash tests/review/run_tests.sh [path/to/p9b.exe]

EXE="${1:-build_vs18/cli/Debug/p9b.exe}"
DIR="tests/review"
TMPDIR="tests/review/tmp"
PASS=0; FAIL=0

mkdir -p "$TMPDIR"
ROOT_ABS="$(pwd)"
EXE_ABS="$ROOT_ABS/$EXE"

run_test() {
    local name="$1" desc="$2" expected="$3" expect_error="${4:-}" stdin_data="${5:-}"
    local bas_file="$ROOT_ABS/$DIR/$name"
    local actual
    if [ -n "$stdin_data" ]; then
        actual=$(cd "$TMPDIR" && echo "$stdin_data" | timeout 10 "$EXE_ABS" "$bas_file" 2>&1 | tr -d '\r')
    else
        actual=$(cd "$TMPDIR" && timeout 10 "$EXE_ABS" "$bas_file" 2>&1 | tr -d '\r')
    fi

    if [ -n "$expect_error" ]; then
        if echo "$actual" | grep -qi "$expect_error"; then
            echo "[PASS] $name  $desc"
            PASS=$((PASS+1))
        else
            echo "[FAIL] $name  $desc  expected_error='$expect_error'  got='$actual'"
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
    rm -f "$TMPDIR"/*.tmp 2>/dev/null
}

echo "=== Review: Bug-fix regression tests ==="

# r01: variables are case-insensitive (x and X must be the same slot)
run_test r01_case_insensitive_vars.bas "Case-insensitive variables" \
    "$(printf '42\n100\n100\nhello')"

# r02: FOR STEP 0 is caught as runtime error; handler calls END
run_test r02_for_step_zero.bas "FOR STEP 0 caught" \
    "$(printf 'caught: step=0')"

# r03: READ type mismatch (string item into numeric var) gives ERR=13
run_test r03_read_type_mismatch.bas "READ type mismatch ERR=13" \
    "$(printf 'mismatch err=13\ndone')"

# r04: negative base ^ fractional exponent gives ERR=5; RESUME NEXT skips to goto done
run_test r04_pow_negative_frac.bas "Pow negative base ERR=5" \
    "$(printf 'caught pow error\n1\ndone')"

# r05: RESUME inside normal flow (no pending error) is handled correctly
run_test r05_resume_without_error.bas "ON ERROR + RESUME NEXT normal flow" \
    "$(printf 'in handler\nafter div0\ndone')"

# r06: DIM with size > 10 000 000 is caught as runtime error
run_test r06_dim_max_size.bas "DIM size cap" \
    "$(printf 'caught dim error\ndone')"

# r07: function return variable lookup is case-insensitive
run_test r07_case_func_return.bas "Function return var case-insensitive" \
    "$(printf '10\n42')"

# r08: 2D array [r,c] access using compile-time column count
run_test r08_array_2d.bas "2D array [r,c] access" \
    "$(printf '11\n14\n23\n34\nAA\nBC')"

# r09: SHR uses logical (unsigned) right shift, not arithmetic
# SHR(-1,1) = 0x7FFFFFFFFFFFFFFF = 9223372036854775807; as double → 9.22337e+18
run_test r09_shr_logical.bas "SHR logical shift (negative values)" \
    "$(printf '8\n2\n0\n9.22337e+18')"

# r10: 3D array [p, r, c] access using compile-time row and column counts
run_test r10_array_3d.bas "3D array [p,r,c] access" \
    "$(printf '111\n123\n134\n211\n234\n234\nAAA\nBBB')"

echo ""
echo "$PASS passed, $FAIL failed"
