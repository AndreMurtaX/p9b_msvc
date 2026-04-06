#!/usr/bin/env bash
# Run all Phase 3 tests.
# Usage: bash tests/phase3/run_tests.sh [path/to/p9b.exe]

EXE="${1:-build_vs18/cli/Debug/p9b.exe}"
DIR="tests/phase3"
TMPDIR="tests/phase3/tmp"
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

echo "=== Phase 3: Quality and Ergonomics ==="

# t01: DATE$() returns 10 chars, TIME$() returns 8 chars; separator is '-' and ':'
run_test t01_date_time.bas  "DATE$/TIME$ format" "$(printf '10\n8\n-\n:')"

# t02: INPUT$(5) reads 5 chars from stdin
run_test t02_input_n.bas    "INPUT\$(n)"          "hello"  "" "hello"

# t03: SHELL executes echo shelltest
run_test t03_shell.bas      "SHELL command"       "shelltest"

# t04: WRITE #n produces CSV-quoted output
run_test t04_write_file.bas "WRITE #n CSV"        '"hello",42,"world"'

# t05: ON ERROR GOTO catches div/0; RESUME NEXT continues
run_test t05_on_error.bas   "ON ERROR GOTO"       "$(printf 'caught error 11\nafter error\ndone')"

# t06: RESUME NEXT skips past error; execution continues after the failing statement
run_test t06_on_error_resume.bas "RESUME NEXT"    "$(printf 'after\ndone')"

# t07: ON ERROR GOTO 0 disables handler
run_test t07_on_error_disable.bas "ON ERROR GOTO 0" "no handler active"

# t08: ERL = line of error; ERR = 11 (div/0)
run_test t08_erl.bas        "ERR and ERL"         "$(printf 'err=11\nerl=3\nok')"

# t09: PRINT USING numeric
run_test t09_print_using_num.bas "PRINT USING ###.##" "$(printf '  3.14\n 42.10\n -7.50')"

# t10: PRINT USING string formats (! = first char, & = whole string)
run_test t10_print_using_str.bas "PRINT USING strings" "$(printf 'H\nWorld\nA')"

# t11: PRINT USING comma in number
run_test t11_print_using_comma.bas "PRINT USING comma" "$(printf '1,234.56\n 9,876')"

# t12: PRINT USING sign (+prefix = always show sign; trailing- = sign after number)
run_test t12_print_using_sign.bas "PRINT USING sign" "$(printf ' +42.00\n -42.00\n  42.00 \n -42.00-')"

# t13: PRINT USING multiple values with literal text
run_test t13_print_using_multi.bas "PRINT USING multi" "Item:  5 Price:  12.99"

# t14: ON ERROR catches file-not-found (ERR=53)
run_test t14_on_error_file.bas "ON ERROR file not found" "$(printf 'caught file error\nerr ok\ndone')"

echo ""
echo "$PASS passed, $FAIL failed"
