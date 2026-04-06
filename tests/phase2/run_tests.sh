#!/usr/bin/env bash
# Run all Phase 2 (File I/O) tests.
# Usage: bash tests/phase2/run_tests.sh [path/to/p9b.exe]

EXE="${1:-build_vs18/cli/Debug/p9b.exe}"
DIR="tests/phase2"
TMPDIR="tests/phase2/tmp"
PASS=0; FAIL=0

mkdir -p "$TMPDIR"

# Resolve absolute paths for cd-based invocation
ROOT_ABS="$(pwd)"
EXE_ABS="$ROOT_ABS/$EXE"

run_test() {
    local name="$1" desc="$2" expected="$3" expect_error="${4:-}"
    # Run from TMPDIR so temp files land there
    local bas_file="$ROOT_ABS/$DIR/$name"
    local actual
    actual=$(cd "$TMPDIR" && timeout 10 "$EXE_ABS" "$bas_file" 2>&1 | tr -d '\r')

    if [ -n "$expect_error" ]; then
        if echo "$actual" | grep -qi "$expect_error"; then
            echo "[PASS] $name  $desc"
            PASS=$((PASS+1))
        else
            echo "[FAIL] $name  $desc  expected error containing '$expect_error'  got='$actual'"
            FAIL=$((FAIL+1))
        fi
    else
        if [ "$actual" = "$expected" ]; then
            echo "[PASS] $name  $desc"
            PASS=$((PASS+1))
        else
            echo "[FAIL] $name  $desc  expected='$expected'  got='$actual'"
            FAIL=$((FAIL+1))
        fi
    fi
    rm -f "$TMPDIR"/*.tmp 2>/dev/null
}

echo "=== File I/O ==="
run_test t01_write_read.bas     "write then read"           "hello world"
run_test t02_append.bas         "append to file"            "$(printf 'line1\nline2')"
run_test t03_eof.bas            "EOF() loop"                "$(printf '10\n20\n30\ndone')"
run_test t04_input_multi.bas    "INPUT #n multi numeric"    "21"
run_test t05_string_vars.bas    "INPUT #n string vars"      "foobar"
run_test t06_line_input.bas     "LINE INPUT whole line"     "hello, world, 42"
run_test t07_close_all.bas      "bare CLOSE all"            "second"
run_test t08_two_files.bas      "two files simultaneous"    "$(printf 'from1\nfrom2')"
run_test t09_print_sep.bas      "PRINT #n semicolon sep"    "abc"
run_test t10_bad_filenum.bas    "error on unopened file"    "" "not open"

echo ""
echo "$PASS passed, $FAIL failed"
