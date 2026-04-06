# Plan9Basic — Language Reference

> **Interpreter version:** 0.8 (C++23 port)
> **Compiler:** MSVC 19.50+ (Visual Studio 2026), C++23
> **Platform:** Windows x64 / Linux x64

---

## Table of Contents

1. [Overview](#1-overview)
2. [Data Types](#2-data-types)
3. [Variables and Naming](#3-variables-and-naming)
4. [Literals](#4-literals)
5. [Operators](#5-operators)
6. [Statements and Commands](#6-statements-and-commands)
   - [Assignment](#61-assignment)
   - [Output — PRINT / PRINTLN](#62-output--print--println)
   - [Input — INPUT](#63-input--input)
   - [Conditionals — IF / ELSEIF / ELSE / ENDIF](#64-conditionals--if--elseif--else--endif)
   - [FOR Loop](#65-for-loop)
   - [WHILE Loop](#66-while-loop)
   - [DO Loop](#67-do-loop)
   - [REPEAT / UNTIL Loop](#68-repeat--until-loop)
   - [BREAK / CONTINUE](#69-break--continue)
   - [GOTO / Labels](#610-goto--labels)
   - [GOSUB / RETURN](#611-gosub--return)
   - [Functions — FUNCTION / ENDFUNCTION](#612-functions--function--endfunction)
   - [LOCAL](#613-local)
   - [Arrays — DIM](#614-arrays--dim)
   - [DATA / READ / RESTORE](#615-data--read--restore)
   - [SELECT CASE](#616-select-case)
   - [ON GOTO / ON GOSUB](#617-on-goto--on-gosub)
   - [File I/O — OPEN / CLOSE / PRINT# / INPUT# / LINE INPUT / EOF](#618-file-io--open--close--print--input--line-input--eof)
   - [WRITE #n — CSV-quoted file output](#619-write-n--csv-quoted-file-output)
   - [ON ERROR GOTO / RESUME / ERR / ERL](#620-on-error-goto--resume--err--erl)
   - [SHELL](#621-shell)
   - [PRINT USING — formatted output](#622-print-using--formatted-output)
   - [LOCATE — cursor positioning](#623-locate--cursor-positioning)
   - [COLOR — terminal color](#624-color--terminal-color)
   - [INCLUDE — file inclusion](#625-include--file-inclusion)
   - [ASSERT — runtime assertion](#626-assert--runtime-assertion)
   - [TRACEON / TRACEOFF — instruction trace](#627-traceon--traceoff--instruction-trace)
   - [END](#628-end)
   - [CLS](#629-cls)
   - [DUMP](#630-dump)
7. [Built-in Functions — Mathematics](#7-built-in-functions--mathematics)
8. [Built-in Functions — Strings](#8-built-in-functions--strings)
9. [Comments](#9-comments)
10. [REPL — Interactive Mode](#10-repl--interactive-mode)
11. [Command-Line Usage](#11-command-line-usage)
12. [Known Limitations](#12-known-limitations)

---

## 1. Overview

Plan9Basic is a structured BASIC interpreter. Source code is compiled to a stack-machine bytecode in a single pass, then executed by a register-less virtual machine.

Key design choices:
- **Case-insensitive keywords** — `PRINT`, `Print`, `print` are identical.
- **Identifier case-insensitive** — `Counter`, `COUNTER`, `counter` are the same variable.
- **Strict type suffix** — variables ending in `$` are strings; all others are numeric (double-precision float).
- **Structured control flow** — no line numbers; block structures have explicit terminators.
- **Single-pass compiler** — forward references to labels/functions are resolved by backpatching.

---

## 2. Data Types

| Type | Storage | Suffix | Range / Notes |
|---|---|---|---|
| **Number** | `double` (64-bit IEEE 754) | *(none)* | ±1.7 × 10³⁰⁸, ~15 significant digits |
| **String** | `std::string` (UTF-8) | `$` | Arbitrary length |

There are no integers as a separate type; integer arithmetic is done in floating-point and values are rounded when displayed as integers.

> **Boolean values** are numeric: `1.0` = true, `0.0` = false. All comparison operators return `1.0` or `0.0`.

---

## 3. Variables and Naming

```
name        → numeric scalar variable    e.g.  counter, x, total
name$       → string scalar variable     e.g.  name$, line$, msg$
name[i]     → numeric array element      e.g.  scores[1], matrix[r,c]
name$[i]    → string array element       e.g.  words$[1], table$[row,col]
```

- Valid characters: letters (`A–Z`, `a–z`), digits (`0–9`), underscore `_`.
- Must start with a letter.
- The `$` suffix must be the last character (before `[` for arrays).
- **Global by default.** Variables declared inside a `FUNCTION` block are local.
- Functions can read and write global variables directly; a new name that appears only inside a function becomes a local variable.
- Scalar and array namespaces are separate — `x` and `x[1]` are independent.
- Maximum 512 global numeric + 512 global string scalar variables.
- Maximum 259 local numeric + 259 local string variables per function.
- Arrays are always global.

### Compile-time undefined-variable detection

The compiler checks variable and array usage at **compile time** and reports an error with the exact source location if:

| Situation | Error message |
|---|---|
| A **global scalar** is read before being assigned anywhere in the main program body | `variable 'x' used before assignment [line L, col C]` |
| An **array** is accessed (read or write) without a preceding `DIM` | `array 'arr' accessed before DIM [line L, col C]` |

Variables that are always guaranteed to be initialised before use are **not** flagged:
- The `FOR` loop control variable (assigned by the loop header).
- Variables filled by `INPUT` or `READ`.
- Function parameters and `LOCAL`-declared variables.
- The **function return variable** (same name as the function — initialised to `0` / `""`).
- Global variables accessed from inside a function body (may be assigned elsewhere in the program).

```basic
' ── Will produce a compile-time error ──────────────────────────────
PRINTLN x          ' error: variable 'x' used before assignment [line 1, col 9]

arr[1] = 5         ' error: array 'arr' accessed before DIM [line 1, col 1]

' ── Correct usage — no error ───────────────────────────────────────
x = 10
PRINTLN x          ' OK

DIM arr[5]
arr[1] = 5         ' OK
```

---

## 4. Literals

### Numeric literals
```basic
42          ' integer
3.14        ' decimal
1.5e10      ' scientific notation
```

### String literals
```basic
"Hello, World!"        ' double-quoted
"He said \"hello\""    ' escaped double quote
"tab:\there"           ' \t, \n, \r, \\ supported
```

### Boolean constants
```basic
TRUE    ' evaluates to 1.0
FALSE   ' evaluates to 0.0
```

---

## 5. Operators

### Arithmetic
| Operator | Description | Example |
|---|---|---|
| `+` | Addition (also string concat) | `a + b` |
| `-` | Subtraction / unary negation | `a - b`, `-x` |
| `*` | Multiplication | `a * b` |
| `/` | Division (float) | `a / b` |
| `MOD` | Modulo (remainder) | `a MOD b` |
| `^` | Power / exponentiation | `a ^ b` |

### String concatenation
| Operator | Description | Example |
|---|---|---|
| `&` | String concatenation | `"Hello " & name$` |
| `+` | String concat (when both operands are strings) | `a$ + b$` |

### Comparison (return 1.0 or 0.0)
| Operator | Description |
|---|---|
| `=` | Equal |
| `<>` | Not equal |
| `<` | Less than |
| `<=` | Less or equal |
| `>` | Greater than |
| `>=` | Greater or equal |

> String comparisons use lexicographic order.

### Logical
| Operator | Description |
|---|---|
| `AND` | Logical AND |
| `OR` | Logical OR |
| `NOT` | Logical NOT |

### Operator precedence (high → low)
1. Unary `-`, `NOT`
2. `^`
3. `*`, `/`, `MOD`
4. `+`, `-`, `&`
5. `=`, `<>`, `<`, `<=`, `>`, `>=`
6. `AND`
7. `OR`

---

## 6. Statements and Commands

### 6.1 Assignment

```basic
LET variable = expression        ' scalar numeric
variable = expression            ' LET is optional
variable$ = string_expression    ' scalar string

name[i] = expression             ' array element (numeric)
name$[i] = string_expression     ' array element (string)
LET name[i] = expression         ' LET is also valid with arrays
LET name$[i] = string_expression
```

Examples:
```basic
LET x = 10
y = x * 2 + 1
name$ = "Alice"
greeting$ = "Hello, " & name$

DIM scores[5]
scores[1] = 100
LET scores[2] = 200

DIM words$[3]
words$[1] = "hello"
LET words$[2] = "world"
```

---

### 6.2 Output — PRINT / PRINTLN

```basic
PRINT expression [; expression ...]   ' print without newline
PRINTLN expression                     ' print with newline
PRINT                                  ' print blank line
```

- **Semicolon `;`** between items: printed with no separator (adjacent).
- **Comma `,`** between items: inserts a TAB character (`\t`) between items — classic BASIC column behaviour.
- A trailing `;` or `,` suppresses the newline on `PRINTLN`.
- Numbers are formatted with the minimum digits needed (no trailing zeros).

Examples:
```basic
PRINTLN "Hello, World!"
PRINT "x = "; x              ' no space between label and value
PRINTLN ""                   ' blank line
PRINT "Result: "; a + b; " done"

PRINTLN x[1], x[2], x[3]    ' TAB-separated: 100    200    300
PRINTLN "Name", "Age"        ' TAB-separated columns
```

---

### 6.3 Input — INPUT

```basic
INPUT variable              ' prompts "? " and reads a number
INPUT variable$             ' prompts "? " and reads a string
INPUT "prompt"; variable    ' custom prompt, single variable
INPUT "prompt"; variable$

INPUT var1, var2, var3      ' multiple variables — one readline per variable
INPUT "prompt"; a, b, c$    ' prompt shown once; each variable read on its own line
```

- The optional string prompt is printed **once** before the first read.
- When reading multiple variables, each variable receives its own line of input (the user presses Enter after each value).
- Numeric variables receive the numeric value of the input line (`0` if not parseable).
- String variables receive the full input line.

Examples:
```basic
INPUT "Enter your name: "; name$
INPUT "Enter a number: "; n

INPUT "Enter width and height: "; w, h
println "Area = " + str$(w * h)

INPUT "x, y, label: "; x, y, label$
```

---

### 6.4 Conditionals — IF / ELSEIF / ELSE / ENDIF

**Single-line form:**
```basic
IF condition THEN statement
IF condition THEN statement ELSE statement
IF condition THEN stmt ELSE IF condition THEN stmt ELSE stmt
```

The single-line form supports full `ELSE IF` chains — all branches on one line.

**Multi-line form:**
```basic
IF condition THEN
  statements
ELSEIF condition THEN     ' also: ELSE IF (two words)
  statements
ELSE
  statements
ENDIF
```

Accepted terminator forms: `ENDIF`, `END IF`.
Accepted ELSEIF forms: `ELSEIF` (one word) and `ELSE IF` (two words) — both are equivalent in both single-line and multi-line forms.
Multiple `ELSEIF` / `ELSE IF` branches are supported in both forms.

Examples:
```basic
' Single-line — no ELSE
IF x > 0 THEN PRINTLN "positive"

' Single-line — with ELSE
IF test < 10 THEN LET a = 10 ELSE LET a = 20

' Single-line — ELSE IF chain
IF a < 5 THEN PRINTLN "small" ELSE IF a < 15 THEN PRINTLN "medium" ELSE PRINTLN "large"

' Multi-line — ELSEIF (one word)
IF score >= 90 THEN
  PRINTLN "A"
ELSEIF score >= 80 THEN
  PRINTLN "B"
ELSE
  PRINTLN "F"
END IF

' Multi-line — ELSE IF (two words), equivalent
IF a < 10 THEN
  PRINTLN "small"
ELSE IF a = 10 THEN
  PRINTLN "exactly 10"
ELSE
  PRINTLN "large"
END IF
```

---

### 6.5 FOR Loop

```basic
FOR variable = start TO limit [STEP increment]
  statements
NEXT [variable]
```

Also accepted: `ENDFOR` and `END FOR` as terminators.

- Default step is `1`.
- A negative step counts downward.
- Loop runs while `variable <= limit` (positive step) or `variable >= limit` (negative step).

Examples:
```basic
FOR i = 1 TO 10
  PRINT i; " "
NEXT i

FOR i = 10 TO 1 STEP -1
  PRINT i; " "
NEXT

FOR i = 0 TO 1 STEP 0.1
  PRINTLN i
END FOR
```

---

### 6.6 WHILE Loop

```basic
WHILE condition
  statements
ENDWHILE
```

Also accepted: `END WHILE`.

- Condition is checked **before** each iteration.
- Loop body is skipped entirely if the condition is false on entry.

Examples:
```basic
LET n = 1
WHILE n <= 100
  LET n = n * 2
ENDWHILE
PRINTLN n

WHILE NOT done
  ' …
END WHILE
```

---

### 6.7 DO Loop

Four forms:

```basic
' Pre-condition WHILE
DO WHILE condition
  statements
LOOP

' Pre-condition UNTIL
DO UNTIL condition
  statements
LOOP

' Post-condition WHILE
DO
  statements
LOOP WHILE condition

' Post-condition UNTIL
DO
  statements
LOOP UNTIL condition
```

- `DO WHILE` / `LOOP WHILE`: continue **while** condition is true.
- `DO UNTIL` / `LOOP UNTIL`: continue **until** condition becomes true.
- Pre-condition form: body may never execute.
- Post-condition form: body always executes at least once.

Examples:
```basic
LET i = 0
DO
  LET i = i + 1
LOOP WHILE i < 5

DO UNTIL EOF
  INPUT line$
  PRINTLN line$
LOOP
```

---

### 6.8 REPEAT / UNTIL Loop

```basic
REPEAT
  statements
UNTIL condition
```

- Always executes the body **at least once**.
- Exits when condition becomes **true**.

Example:
```basic
REPEAT
  INPUT "Enter positive number: "; n
UNTIL n > 0
```

---

### 6.9 BREAK / CONTINUE

```basic
BREAK        ' exit the innermost loop immediately
CONTINUE     ' skip to the next iteration of the innermost loop
```

Both work inside `FOR`, `WHILE`, `DO`, and `REPEAT` loops.

Examples:
```basic
FOR i = 1 TO 10
  IF i = 5 THEN BREAK
  PRINTLN i
NEXT i
' prints 1 2 3 4

FOR i = 1 TO 10
  IF i MOD 2 = 0 THEN CONTINUE
  PRINTLN i
NEXT i
' prints 1 3 5 7 9
```

---

### 6.10 GOTO / Labels

```basic
label:                   ' define a label (colon at the end)
GOTO label               ' unconditional jump
```

- Labels are global.
- Forward references are resolved by backpatching at the end of compilation.

Example:
```basic
GOTO skip
PRINTLN "This is skipped"
skip:
PRINTLN "Jumped here"
```

---

### 6.11 GOSUB / RETURN

```basic
GOSUB label       ' call a subroutine (saves return address)
label:
  statements
RETURN            ' return to the instruction after GOSUB
```

- Unlike `FUNCTION`, `GOSUB` shares the global variable scope.
- `RETURN` without a value inside a subroutine returns to the caller.

Example:
```basic
GOSUB greet
GOTO done

greet:
  PRINTLN "Hello from subroutine!"
  RETURN

done:
PRINTLN "Back in main."
```

---

### 6.12 Functions — FUNCTION / ENDFUNCTION

```basic
FUNCTION name(param1, param2, ...) [LOCAL var1, var2$, ...]
  statements
  RETURN expression
ENDFUNCTION
```

- **Numeric function:** name without `$` — returns a number.
- **String function:** name ending with `$` — returns a string.
- Parameters and local variables are **local** to the function.
- Global variables are accessible (read and write) from inside a function.
- Mixing numeric and string parameters is allowed.
- Both `ENDFUNCTION` and `END FUNCTION` are accepted.
- The `LOCAL` declaration can appear on the same header line or as a separate statement in the body.

**Two equivalent ways to return a value:**

1. `RETURN expression` — explicit return with a value (exits immediately).
2. Assign to a variable with the **same name as the function** inside the body; execution then falls through to `ENDFUNCTION` and the assigned value is returned.

If neither form is used, the function returns `0` (numeric) or `""` (string).

Examples:
```basic
' ── Explicit RETURN ────────────────────────────────────────────────
FUNCTION square(x)
  RETURN x * x
END FUNCTION

FUNCTION greet$(name$)
  RETURN "Hello, " & name$ & "!"
ENDFUNCTION

' ── Function-name assignment (BASIC classic style) ─────────────────
FUNCTION add(a, b)
  add = a + b           ' assigns to return variable
ENDFUNCTION             ' returns the assigned value

FUNCTION repeat$(s$, n) LOCAL i
  repeat$ = ""
  FOR i = 1 TO n
    repeat$ = repeat$ + s$
  NEXT i
ENDFUNCTION

' ── Global variable access ──────────────────────────────────────────
total = 0
FUNCTION accumulate(n)
  total = total + n     ' reads and writes the global 'total'
  accumulate = total
ENDFUNCTION

PRINTLN square(5)           ' 25
PRINTLN greet$("World")     ' Hello, World!
PRINTLN add(3, 7)           ' 10
PRINTLN repeat$("AB", 3)    ' ABABAB
accumulate(10) : accumulate(20)
PRINTLN total               ' 30
```

---

### 6.13 LOCAL

```basic
LOCAL var1, var2$, var3, ...
```

- Declares local variables inside a function body.
- Can also appear on the same line as the `FUNCTION` header.
- Local variables are invisible outside the function.
- Numeric locals default to `0.0`, string locals to `""`.

---

### 6.14 Arrays — DIM

Arrays must be declared with `DIM` before first use.

**Declaration:**
```basic
DIM name[size]                ' 1-D numeric array with 'size' elements
DIM name$[size]               ' 1-D string array
DIM name[rows, cols]          ' 2-D numeric array  (stored flat: rows×cols)
DIM name$[rows, cols]         ' 2-D string array
DIM name[planes, rows, cols]  ' 3-D numeric array  (stored flat: planes×rows×cols)
DIM name$[planes, rows, cols] ' 3-D string array
DIM a[10], b$[5], c[4,4,4]   ' multiple arrays in one DIM statement
```

**Element access (1-based index):**
```basic
name[i]              ' 1-D: read element i
name[i] = expr       ' 1-D: write element i  (with or without LET)

name[r, c]           ' 2-D: read element at row r, column c
name[r, c] = expr    ' 2-D: write element at row r, column c

name[p, r, c]        ' 3-D: read element at plane p, row r, column c
name[p, r, c] = expr ' 3-D: write element at plane p, row r, column c
```

**Flat-index formulas (all indices are 1-based):**

| Dimensions | Formula |
|---|---|
| 1-D | `i` |
| 2-D | `(r − 1) × cols + c` |
| 3-D | `((p − 1) × rows + (r − 1)) × cols + c` |

**Rules:**
- Index is **1-based**: valid range is `1 .. size` (runtime bounds-checked).
- `DIM` may appear on the same line as other statements separated by `:`.
- Accessing an array that was not `DIM`-ed is caught at **compile time** and reported as an error with source location (e.g. `array 'arr' accessed before DIM [line 3, col 1]`).
- Array names live in a separate namespace from scalar variables — `x` (scalar) and `x[1]` (array) coexist independently.
- **Multi-dim literal requirement:** for `[r, c]` and `[p, r, c]` syntax to work, the dimension sizes other than the first **must be numeric literal constants** in the `DIM` statement (e.g. `DIM m[3, 4]` or `DIM v[2, 3, 4]`). Using a variable expression for those sizes is allowed for allocation purposes but disables the multi-index syntax; only the flat single-index form `name[i]` will work in that case.

Examples:
```basic
' --- 1-D numeric array ---
DIM scores[5]
FOR i = 1 TO 5
  scores[i] = i * 10
NEXT i
PRINTLN scores[3]                ' 30

' --- 1-D string array with LET ---
DIM names$[3]
LET names$[1] = "Alice"
LET names$[2] = "Bob"
LET names$[3] = "Carol"
PRINTLN names$[2]                ' Bob

' --- Iterate a string array ---
DIM greets$[2]
greets$[1] = "Hello world in"
LET greets$[2] = "2026"
FOR i = 1 TO 2
  PRINTLN greets$[i]
NEXT i
' Hello world in
' 2026

' --- 2-D matrix (3 rows × 4 cols) ---
DIM m[3, 4]
FOR r = 1 TO 3
  FOR c = 1 TO 4
    m[r, c] = r * 10 + c
  NEXT c
NEXT r
PRINTLN m[2, 3]                  ' 23

' --- 3-D cube (2 planes × 3 rows × 4 cols) ---
DIM cube[2, 3, 4]
FOR p = 1 TO 2
  FOR r = 1 TO 3
    FOR c = 1 TO 4
      cube[p, r, c] = p * 100 + r * 10 + c
    NEXT c
  NEXT r
NEXT p
PRINTLN cube[1, 2, 3]            ' 123
PRINTLN cube[2, 3, 4]            ' 234

' --- DIM + assignment on same line ---
DIM x[3] : x[1] = 100 : x[2] = 200 : x[3] = 300
PRINTLN x[1], x[2], x[3]        ' 100 200 300

' --- Multiple arrays in one DIM ---
DIM a[10], b$[5]
a[1] = 42 : b$[1] = "hello"
```

---

### 6.15 DATA / READ / RESTORE

```basic
DATA value1, value2, "string", ...
READ variable
READ variable$
RESTORE
```

- `DATA` lines define a sequential list of constants (embedded in the bytecode).
- `READ` assigns the next value from the data stream to a variable.
- Type must match: `READ n` reads a number; `READ s$` reads a string.
- `RESTORE` resets the data pointer to the beginning.
- `DATA` statements can appear anywhere in the program; they are all collected before execution.

Example:
```basic
DATA 10, 20, 30, "Alice", "Bob"
READ a
READ b
READ c
READ name1$
READ name2$
PRINTLN a + b + c     ' 60
PRINTLN name1$        ' Alice
RESTORE
READ a                ' reads 10 again
```

---

### 6.16 SELECT CASE

Evaluates an expression once and dispatches to the matching clause.

```basic
SELECT CASE expression
  CASE value
    statements
  CASE value1, value2, value3      ' list — any of these values
    statements
  CASE lower TO upper              ' range — lower ≤ expr ≤ upper
    statements
  CASE IS relop value              ' relational — expr relop value
    statements
  CASE ELSE                        ' default (optional; must be last)
    statements
END SELECT    ' also accepted: ENDSELECT
```

- The selector `expression` may be **numeric** or **string** — all `CASE` items are compared accordingly.
- `CASE` clauses are tested **top to bottom**; only the first matching clause executes.
- Items in a list or mixed forms can be **combined with commas**: `CASE 1, 3 TO 5, IS > 10`
- `CASE IS` accepts: `=`, `<>`, `<`, `<=`, `>`, `>=`
- `CASE ELSE` matches everything; must be the last clause if present.
- `SELECT CASE` blocks can be **nested**.
- No `BREAK` is needed — only one clause ever executes.

Examples:
```basic
' ── Numeric selector ───────────────────────────────────────────────
SELECT CASE score
  CASE IS >= 90
    println "A"
  CASE 80 TO 89
    println "B"
  CASE 70, 71, 72, 73, 74, 75, 76, 77, 78, 79
    println "C"
  CASE ELSE
    println "Below C"
END SELECT

' ── String selector ────────────────────────────────────────────────
SELECT CASE cmd$
  CASE "quit", "exit", "bye"
    println "Goodbye!"
  CASE "help"
    println "Commands: quit, help"
  CASE ELSE
    println "Unknown command: " + cmd$
END SELECT

' ── Nested SELECT ───────────────────────────────────────────────────
SELECT CASE category
  CASE 1
    SELECT CASE sub
      CASE 1 : println "1-1"
      CASE 2 : println "1-2"
    END SELECT
  CASE 2
    println "category 2"
END SELECT
```

---

### 6.17 ON GOTO / ON GOSUB

Computed jump or subroutine call based on a 1-based integer expression.

```basic
ON expression GOTO  label1 [, label2, ...]
ON expression GOSUB label1 [, label2, ...]
```

- `expression` is evaluated to an integer index (1-based).
- If the value is `1`, jumps to `label1`; if `2`, to `label2`; etc.
- If the value is `0`, negative, or greater than the number of labels, execution **falls through** to the next statement (no error).
- `ON … GOTO` is an unconditional jump; `ON … GOSUB` calls a subroutine and returns after the `RETURN` statement.
- Labels are written with or without the trailing colon.

Examples:
```basic
' ── ON GOTO ────────────────────────────────────────────────────────
x = 2
ON x GOTO lbl1:, lbl2:, lbl3:
PRINTLN "out of range"
END

lbl1: PRINTLN "one"   : END
lbl2: PRINTLN "two"   : END
lbl3: PRINTLN "three" : END

' ── ON GOSUB ───────────────────────────────────────────────────────
result$ = ""
FOR i = 1 TO 3
  ON i GOSUB sub_a:, sub_b:, sub_c:
NEXT i
PRINTLN result$   ' prints ABC
END

sub_a: result$ = result$ + "A" : RETURN
sub_b: result$ = result$ + "B" : RETURN
sub_c: result$ = result$ + "C" : RETURN
```

---

### 6.18 File I/O — OPEN / CLOSE / PRINT# / INPUT# / LINE INPUT / EOF

Plan9Basic supports sequential file I/O modelled on GW-BASIC. Files are identified by a **file number** `#n` (integer ≥ 1). Up to any number of files may be open simultaneously (limited by the OS).

#### OPEN

```basic
OPEN "filename" FOR INPUT  AS #n    ' open for reading
OPEN "filename" FOR OUTPUT AS #n    ' open for writing (truncates existing file)
OPEN "filename" FOR APPEND AS #n    ' open for writing at end-of-file
```

- Opens the named file and associates it with file number `n`.
- Opening an already-open file number is a runtime error.
- `OUTPUT` truncates the file if it exists; `APPEND` preserves existing content.

#### CLOSE

```basic
CLOSE #n            ' close specific file
CLOSE #n, #m        ' close multiple files
CLOSE               ' close all open files
```

#### PRINT #n

```basic
PRINT #n, expr [; expr ...]   ' write value(s) to file; newline at end
PRINT #n, expr ;              ' trailing semicolon suppresses newline
PRINT #n, expr , expr         ' comma inserts TAB between items
PRINT #n,                     ' write empty line (newline only)
```

Mirrors the `PRINT` statement syntax but writes to the file associated with `#n`.

#### INPUT #n

```basic
INPUT #n, var           ' read one line from file into numeric variable
INPUT #n, var$          ' read one line from file into string variable
INPUT #n, a, b$, c      ' read one line per variable (multiple vars)
```

Each variable reads one complete line from the file (similar to `std::getline`). The line is converted to a number for numeric variables.

#### LINE INPUT #n

```basic
LINE INPUT #n, var$
```

Reads an entire line (including any commas) from the file into a string variable. Equivalent to `INPUT #n` for strings but more explicit; useful when the line content may contain commas.

#### EOF()

```basic
if eof(1) <> 0 then ...   ' true (-1) when file #1 is at end-of-file
while eof(n) = 0
  ...
wend
```

`EOF(n)` is a numeric function. Returns **-1** (true) when the file associated with `#n` has no more data to read, **0** (false) otherwise. Evaluated before each read to avoid reading past the end.

#### Complete Example

```basic
' ── Write a CSV-style file ──────────────────────────────────────────────────
OPEN "data.txt" FOR OUTPUT AS #1
FOR i = 1 TO 5
  PRINT #1, str$(i) + "," + str$(i * i)
NEXT i
CLOSE #1

' ── Read it back ────────────────────────────────────────────────────────────
OPEN "data.txt" FOR INPUT AS #1
WHILE EOF(1) = 0
  LINE INPUT #1, row$
  PRINTLN row$
WEND
CLOSE #1
```

#### Notes

- File numbers are small positive integers (1, 2, 3 …); there is no practical upper limit.
- `CLOSE` without arguments is the safest way to ensure all files are flushed and closed before the program ends.
- Runtime error messages include the file number (e.g. `File #3 is not open`).

---

### 6.19 WRITE #n — CSV-quoted file output

```basic
WRITE #n, expr [, expr ...]
```

Writes values to the file in CSV-quoted format. Strings are always enclosed in double quotes; numbers are written unquoted. Values are separated by commas. A newline is appended at the end.

This matches the GW-BASIC `WRITE #` behaviour and is ideal for producing files that can later be read by `INPUT #` or third-party tools.

Example:
```basic
OPEN "out.csv" FOR OUTPUT AS #1
WRITE #1, "hello", 42, "world"   ' produces → "hello",42,"world"
CLOSE #1
```

---

### 6.20 ON ERROR GOTO / RESUME / ERR / ERL

Plan9Basic supports structured runtime error handling modelled on GW-BASIC.

#### Enabling / Disabling a Handler

```basic
ON ERROR GOTO label:    ' enable; redirect runtime errors to label
ON ERROR GOTO 0         ' disable the current error handler
```

- When a runtime error occurs (division by zero, file not found, etc.) and a handler is active, execution jumps to `label` instead of stopping.
- Only one handler is active at a time; `ON ERROR GOTO label` replaces any previously installed handler.
- `ON ERROR GOTO 0` removes the handler; subsequent errors will stop the program normally.

#### Resuming After an Error

```basic
RESUME            ' retry the failed statement
RESUME NEXT       ' continue at the statement after the one that failed
RESUME label:     ' continue at a specific label
```

- `RESUME` and `RESUME NEXT` may only appear inside an error handler.
- After `RESUME` (or any of its variants), the handler becomes re-enterable — if another error occurs, the handler will be invoked again.

#### ERR and ERL

```basic
ERR     ' numeric: GW-BASIC-style error code of the most recent error
ERL     ' numeric: source line number where the error occurred
```

Common error codes:

| Code | Meaning |
|---|---|
| 5  | Illegal function call (stack underflow, bad argument) |
| 11 | Division by zero |
| 53 | File not found |
| 64 | Bad file number (file not open) |

Example:
```basic
ON ERROR GOTO handler:
x = 10 / 0
PRINTLN "after error, x = "; x
GOTO done:

handler:
  PRINTLN "caught error "; ERR; " at line "; ERL
  RESUME NEXT

done:
PRINTLN "done"
' Output:
'   caught error 11 at line 2
'   after error, x = 0
'   done
```

---

### 6.21 SHELL

```basic
SHELL "command"
SHELL command_string$
```

Executes an OS shell command synchronously. The interpreter waits for the command to finish before continuing.

- On Windows the command runs via `cmd.exe`; on Linux/macOS via `/bin/sh`.
- The exit code of the command is not captured.
- Use `SHELL "echo text"` to print to the console from outside the interpreter.

Example:
```basic
SHELL "echo Hello from shell"
SHELL "dir"                     ' list files (Windows)
SHELL "ls -la"                  ' list files (Linux/macOS)
```

---

### 6.22 PRINT USING — formatted output

`PRINT USING` formats one or more values with a template string, similar to GW-BASIC `PRINT USING`.

```basic
PRINT USING format$; expr [; expr ...]
PRINT USING format$; expr [; expr ...];   ' trailing ; suppresses newline
```

The format string is scanned left-to-right. Each format specifier consumes one value. Literal characters before the specifier are printed verbatim. After all values are consumed the rest of the format string (trailing literals) is printed.

#### Numeric Specifiers

| Characters | Meaning |
|---|---|
| `#` | One digit position (leading spaces if unused) |
| `.` | Decimal point |
| `,` | Comma-separate groups of 3 digits in the integer part |
| `+` (leading) | Always show sign (`+` for positive, `-` for negative) |
| `-` (trailing) | Show trailing `-` for negative; space for positive |
| `$` | Prefix output with `$` |

Field width is the total number of `#` characters plus decoration (decimal point, commas, sign, `$`). Numbers are right-justified and padded with spaces. If the number overflows the field, a `%` prefix is prepended.

#### String Specifiers

| Characters | Meaning |
|---|---|
| `!` | Print only the first character of the string |
| `&` | Print the entire string (variable width) |
| `\` _spaces_ `\` | Print string left-justified in a fixed-width field; field width = number of spaces + 2 |

Examples:
```basic
PRINT USING "###.##"; 3.14159    ' output:   3.14
PRINT USING "###.##"; 42.1       ' output:  42.10
PRINT USING "###.##"; -7.5       ' output:  -7.50

PRINT USING "#,###.##"; 1234.56  ' output: 1,234.56
PRINT USING "+###.##"; 42.0      ' output:  +42.00
PRINT USING "+###.##"; -42.0     ' output:  -42.00
PRINT USING "###.##-"; -42.0     ' output:  -42.00-

PRINT USING "!"; "Hello"         ' output: H
PRINT USING "&"; "World"         ' output: World

PRINT USING "Item: ## Price: ###.##"; 5; 12.99
' output: Item:  5 Price:  12.99
```

---

### 6.23 END

```basic
END
```

Terminates program execution immediately. May appear anywhere.

---

### 6.24 CLS

```basic
CLS
```

Clears the console screen (sends ANSI escape `\033[2J\033[H`).

---

### 6.25 DUMP

```basic
DUMP
```

Prints a debug snapshot of the current instruction pointer and stack depth to stdout. Useful for tracing execution.

---

### 6.23 LOCATE — cursor positioning

```basic
LOCATE row, col
```

Moves the terminal cursor to the given row and column by emitting an ANSI escape sequence (`\033[row;colH`). Both `row` and `col` are 1-based integers. Useful for building text-mode UIs.

Example:
```basic
LOCATE 5, 10
PRINT "Hello at row 5, col 10"
LOCATE 1, 1     ' home position
```

> **Note:** Requires a terminal that supports ANSI escape codes. Has no effect when output is redirected to a file.

---

### 6.24 COLOR — terminal color

```basic
COLOR fg           ' set foreground color only
COLOR fg, bg       ' set foreground and background color
COLOR 0            ' reset to default (index 0 = black fg — use COLOR 7 for white)
```

Sets the terminal text color using ANSI escape sequences.

**Foreground color codes (`fg`):**

| Code | Color | Code | Color |
|---|---|---|---|
| 0 | Black | 8 | Dark Gray |
| 1 | Red | 9 | Bright Red |
| 2 | Green | 10 | Bright Green |
| 3 | Yellow / Brown | 11 | Bright Yellow |
| 4 | Blue | 12 | Bright Blue |
| 5 | Magenta | 13 | Bright Magenta |
| 6 | Cyan | 14 | Bright Cyan |
| 7 | Light Gray | 15 | White |

**Background color codes (`bg`):** 0–7 (same as fg 0–7 column).

To reset all colors: emit `COLOR 7` (white on default) or use ANSI reset directly via `PRINT CHR$(27) + "[0m"`.

Example:
```basic
COLOR 10          ' bright green text
PRINTLN "Success!"
COLOR 9, 0        ' bright red on black
PRINTLN "Error!"
COLOR 7           ' back to normal
```

---

### 6.25 INCLUDE — file inclusion

```basic
INCLUDE "filename.bas"
INCLUDE "path/to/library.bas"
```

Includes and compiles the contents of another BASIC source file at the point of the `INCLUDE` directive. The included file is expanded inline before compilation begins (text preprocessor approach).

- Path is relative to the **directory of the file containing the `INCLUDE`** (file mode) or the **current working directory** where `p9b` was launched (REPL mode).
- Absolute paths are also accepted.
- Circular inclusion is detected and stopped at a nesting depth of **16 levels**.
- The included file can itself contain `INCLUDE` directives.
- Works in both file mode and interactive REPL mode.

Example:

`mathlib.bas`:
```basic
function clamp(val, lo, hi)
  if val < lo then clamp = lo : return val
  if val > hi then clamp = hi : return val
  clamp = val
endfunction
```

`main.bas`:
```basic
include "mathlib.bas"

println clamp(150, 0, 100)   ' 100
println clamp(-5, 0, 100)    ' 0
println clamp(42, 0, 100)    ' 42
```

---

### 6.26 ASSERT — runtime assertion

```basic
ASSERT condition
ASSERT condition, message$
```

Evaluates `condition`. If it is **false** (0), raises a runtime error (code 5). If an `ON ERROR` handler is active, execution jumps to it; otherwise the program stops with an error message.

- Without `message$`: error text is `"Assertion failed at line N"`.
- With `message$`: the custom message is used as the error text.
- `ASSERT` integrates with `ON ERROR GOTO` — assertions can be caught and handled.

Examples:
```basic
ASSERT x > 0                        ' stops if x <= 0
ASSERT n >= 1 AND n <= 100, "n must be 1..100"

' Guarded assertion
ON ERROR GOTO fail:
ASSERT len(name$) > 0, "name cannot be empty"
GOTO ok:
fail:
  PRINTLN "Validation error"
  END
ok:
  PRINTLN "name is: " + name$
```

---

### 6.27 TRACEON / TRACEOFF — instruction trace

```basic
TRACEON    ' start printing one line per bytecode instruction executed
TRACEOFF   ' stop tracing
```

When tracing is enabled, every instruction executed prints a diagnostic line to stdout:

```
[TRACE] ln=5 op=42
```

Where `ln` is the source line number and `op` is the internal bytecode opcode number. Trace output is interleaved with normal `PRINT`/`PRINTLN` output.

Use `TRACEON`/`TRACEOFF` around a specific section of code to limit output volume:

```basic
x = 0
TRACEON
FOR i = 1 TO 3
  x = x + i
NEXT i
TRACEOFF
PRINTLN x    ' 6
```

> **Tip:** Pipe output through a file or tool to analyse the trace: `p9b prog.bas > trace.txt`.

---

### 6.28 END

```basic
END
```

Terminates program execution immediately. May appear anywhere.

---

### 6.29 CLS

```basic
CLS
```

Clears the console screen (sends ANSI escape `\033[2J\033[H`).

---

### 6.30 DUMP

```basic
DUMP
```

Prints a debug snapshot of the current instruction pointer and stack depth to stdout. Useful for tracing execution manually.

---

## 7. Built-in Functions — Mathematics

All numeric math functions are registered with signatures like `ABS@N` (one numeric arg) or `ROUND@NN` (two numeric args).

| Function | Signature | Description |
|---|---|---|
| `ABS(x)` | `ABS@N` | Absolute value |
| `SQR(x)` | `SQR@N` | Square root |
| `SIN(x)` | `SIN@N` | Sine (x in radians) |
| `COS(x)` | `COS@N` | Cosine (x in radians) |
| `TAN(x)` | `TAN@N` | Tangent (x in radians) |
| `ATN(x)` | `ATN@N` | Arc-tangent (result in radians) |
| `LOG(x)` | `LOG@N` | Natural logarithm (base e) |
| `LOG2(x)` | `LOG2@N` | Logarithm base 2 |
| `LOG10(x)` | `LOG10@N` | Logarithm base 10 |
| `EXP(x)` | `EXP@N` | e raised to the power x |
| `INT(x)` | `INT@N` | Floor toward −∞ (truncates toward negative infinity) |
| `FIX(x)` | `FIX@N` | Truncate toward zero |
| `CEIL(x)` | `CEIL@N` | Ceiling (smallest integer ≥ x) |
| `FLOOR(x)` | `FLOOR@N` | Floor (largest integer ≤ x) |
| `SGN(x)` | `SGN@N` | Sign: −1, 0, or 1 |
| `ROUND(x)` | `ROUND@N` | Round to nearest integer |
| `ROUND(x, d)` | `ROUND@NN` | Round to `d` decimal places |
| `RND()` | `RND@` | Random float in [0, 1) |
| `RND(n)` | `RND@N` | Random float in [0, n) |
| `PI()` | `PI@` | π ≈ 3.14159265358979… |
| `MAX(a, b)` | `MAX@NN` | Larger of two numbers |
| `MIN(a, b)` | `MIN@NN` | Smaller of two numbers |
| `POW(x, y)` | `POW@NN` | x raised to the power y (same as `x ^ y`) |
| `CLNG(x)` | `CLNG@N` | Truncate to integer toward zero (e.g. `CLNG(-3.9)` → `-3`) |

### Bitwise Operations

Operate on the integer part of their arguments (64-bit two's complement).

| Function | Signature | Description |
|---|---|---|
| `BAND(a, b)` | `BAND@NN` | Bitwise AND |
| `BOR(a, b)` | `BOR@NN` | Bitwise OR |
| `BXOR(a, b)` | `BXOR@NN` | Bitwise XOR |
| `BNOT(a)` | `BNOT@N` | Bitwise NOT (one's complement) |
| `SHL(a, n)` | `SHL@NN` | Shift `a` left by `n` bits |
| `SHR(a, n)` | `SHR@NN` | Shift `a` right by `n` bits (arithmetic) |

### Timer

| Function | Description |
|---|---|
| `TIMER` | Elapsed seconds (double) since the program started. Used as a bare word — no parentheses. |

Examples:
```basic
PRINTLN SQR(2)            ' 1.41421356...
PRINTLN ROUND(3.14159, 2) ' 3.14
PRINTLN RND(100)          ' random 0..99
PRINTLN PI()              ' 3.14159265358979
PRINTLN MAX(7, 3)         ' 7
PRINTLN CLNG(-3.9)        ' -3
PRINTLN BAND(12, 10)      ' 8   (1100 AND 1010 = 1000)
PRINTLN BOR(12, 10)       ' 14  (1100 OR  1010 = 1110)
PRINTLN BXOR(12, 10)      ' 6   (1100 XOR 1010 = 0110)
PRINTLN BNOT(0)           ' -1
PRINTLN SHL(1, 4)         ' 16
PRINTLN SHR(16, 2)        ' 4
t0 = TIMER
' ... do work ...
PRINTLN "elapsed: " + STR$(TIMER - t0) + " s"
```

---

## 8. Built-in Functions — Strings

String-returning functions end with `$`.

### Date and Time
| Function | Description | Example output |
|---|---|---|
| `DATE$()` | Current date as `"MM-DD-YYYY"` (10 chars) | `"04-05-2026"` |
| `TIME$()` | Current time as `"HH:MM:SS"` (8 chars) | `"14:30:00"` |

### Keyboard / Input
| Function | Description |
|---|---|
| `INKEY$()` | Returns the next keyboard character without blocking; `""` if no key is available. Works on Windows and Linux. |
| `INPUT$(n)` | Reads exactly `n` characters from standard input. |

### Inspection
| Function | Description |
|---|---|
| `LEN(s$)` | Length in characters |
| `ASC(s$)` | ASCII code of the first character |
| `VAL(s$)` | Convert string to number (0 if not numeric) |

### Extraction
| Function | Description |
|---|---|
| `LEFT$(s$, n)` | First `n` characters |
| `RIGHT$(s$, n)` | Last `n` characters |
| `MID$(s$, start, len)` | Substring: `len` characters from 1-based position `start` |
| `MID$(s$, start)` | Substring from `start` to end |

### Search
| Function | Description |
|---|---|
| `INSTR(s$, find$)` | 1-based position of `find$` in `s$`; 0 if not found |
| `INSTR(start, s$, find$)` | Same, starting from position `start` |

### Conversion
| Function | Description |
|---|---|
| `STR$(n)` | Number to string (shortest representation) |
| `CHR$(n)` | Character with ASCII code `n` |
| `UPPER$(s$)` | Convert to uppercase |
| `LOWER$(s$)` | Convert to lowercase |
| `HEX$(n)` | Number to hexadecimal string (e.g., `"1F"`) |
| `BIN$(n)` | Number to binary string (e.g., `"11011"`) |

### Trimming
| Function | Description |
|---|---|
| `TRIM$(s$)` | Remove leading and trailing whitespace |
| `LTRIM$(s$)` | Remove leading whitespace |
| `RTRIM$(s$)` | Remove trailing whitespace |

### Manipulation
| Function | Description |
|---|---|
| `REPLACE$(s$, find$, repl$)` | Replace all occurrences of `find$` with `repl$` |
| `REPEAT$(s$, n)` | Repeat string `s$` `n` times |
| `LPAD$(s$, n)` | Left-pad with spaces to width `n` |
| `RPAD$(s$, n)` | Right-pad with spaces to width `n` |
| `SPLIT$(s$, delim$, n)` | Return the `n`-th token (0-based) obtained by splitting `s$` on `delim$`; returns `""` if `n` is out of range |
| `SPACE$(n)` | String of `n` space characters |
| `STRING$(n, c$)` | String of `n` repetitions of the first character of `c$` |

### Formatting

| Function | Description |
|---|---|
| `FORMAT$(n, fmt$)` | Format number `n` using a `PRINT USING` format string; returns the result as a string instead of printing it. Supports all numeric format specifiers (`#`, `.`, `,`, `+`, `-`, `$`). |

Examples:
```basic
PRINTLN LEN("Hello")              ' 5
PRINTLN LEFT$("Hello", 3)        ' Hel
PRINTLN MID$("Hello", 2, 3)      ' ell
PRINTLN INSTR("Hello", "ll")     ' 3
PRINTLN STR$(3.14)               ' 3.14
PRINTLN UPPER$("hello")          ' HELLO
PRINTLN TRIM$("  hi  ")          ' hi
PRINTLN REPEAT$("ab", 4)         ' abababab
PRINTLN HEX$(255)                ' FF
PRINTLN CHR$(65)                 ' A
PRINTLN SPACE$(5)                ' "     "
PRINTLN STRING$(4, "*")          ' "****"
PRINTLN FORMAT$(3.14159, "###.##")  '   3.14
PRINTLN FORMAT$(1234.5, "#,###.##") ' 1,234.50
PRINTLN "[" + SPACE$(3) + "]"    ' [   ]
```

---

## 9. Comments

```basic
' This is a comment (single quote)
REM This is also a comment
```

Comments extend to the end of the line. There are no block comments.

---

## 10. REPL — Interactive Mode

Run `p9b` with no arguments to start the interactive REPL.

```
Plan9Basic 0.8 (Apr 5 2026, ...) [MSVC ..., 64-bit] on win32
Type ".help" for help, ".quit" to exit.

>>>
```

**Workflow:**
1. Type BASIC lines — they accumulate in a buffer (prompt shows `...`).
2. Press **Enter on an empty line** to compile and execute the entire buffer.
3. All lines in the buffer share the same execution context.

```
>>> let a = 2026
... let hello$ = "Hello world in "
... println hello$ & STR$(a)
...
Hello world in 2026
>>>
```

**REPL commands** (prefix with `.`):

| Command | Description |
|---|---|
| `.help` | Show help text |
| `.funcs` | List all registered built-in functions with signatures |
| `.run` | Compile and execute the current buffer |
| `.check` | Tokenize the buffer and display the token stream |
| `.show` | Print the current program buffer |
| `.clear` | Clear the program buffer |
| `.tokens` | Toggle per-line token display |
| `.quit` or `.exit` | Exit the REPL |

---

## 11. Command-Line Usage

```
p9b                        Interactive REPL
p9b <file.bas>             Compile and run a BASIC file
p9b --tokens <file.bas>    Tokenize file and dump the full token stream
p9b --funcs                List all registered built-in functions
p9b --help                 Show help
```

Examples:
```bash
p9b hello.bas
p9b --tokens myprog.bas
p9b --funcs | findstr STR
```

Exit code is `0` on success, `1` if any file produced an error.

---

## 12. Known Limitations

| Area | Limitation |
|---|---|
| **Arrays — local** | Arrays are always global; local (function-scoped) arrays are not supported. |
| **Arrays — multi-dim 4+** | Up to 3-D arrays are supported (`DIM name[p,r,c]`). Four or more dimensions are not supported. |
| **Undefined-variable check — locals** | Inside a function, only variables explicitly known to be local (parameters, `LOCAL`-declared, function return variable) are checked for use-before-assignment. Other names fall back to global lookup and are not checked at compile time. |
| **INPUT — single-line multi-value** | Multi-variable `INPUT a, b` reads one variable per line; it does not parse a comma-separated list from a single input line. |
| **File I/O — random access** | Only sequential access (`INPUT` / `OUTPUT` / `APPEND`). Random-access (`RANDOM`, `GET`, `PUT`) is not supported. |
| **File I/O — multi-value INPUT#** | `INPUT #n, a, b` reads one variable per line; it does not parse a comma-delimited list from a single line. |
| **INKEY$() — macOS** | `INKEY$()` works on Windows (via `_kbhit`/`_getch`) and Linux (via `termios` raw mode). On macOS it may require additional terminal configuration. |
| **SHELL — exit code** | The exit code of the shell command is not captured or accessible from BASIC. |
| **PRINT USING / FORMAT$ — `\…\` in string literals** | The backslash `\` is an escape character inside BASIC string literals. To use a `\…\` format string, construct it with `CHR$(92)`. |
| **ON ERROR — nested errors** | If an error occurs inside an error handler (before `RESUME`), the original handler is not re-entered; the program stops with the new error. |
| **LOCATE / COLOR — terminal** | LOCATE and COLOR emit raw ANSI escapes; they have no effect when output is redirected to a file or a terminal that does not support ANSI. |
| **INCLUDE — REPL base dir** | In the REPL, `INCLUDE` resolves paths relative to the **current working directory** (where `p9b` was launched), not the directory of the executable. Place included files in the working directory or use absolute paths. |
| **TRACE — performance** | `TRACEON` produces one output line per bytecode instruction; for tight loops this can generate thousands of lines per second. Use `TRACEOFF` to limit the scope. |
| **ASSERT — error code** | `ASSERT` triggers error code 5 (Illegal function call). If `ON ERROR` is active, the handler will be invoked and can inspect `ERR` and `ERL`. |
| **Persistent REPL state** | Each buffer execution creates a new VM; variables do not persist between separate submissions. Use one buffer for all related code. |
| **Timeout / step limit** | No execution timeout enforced yet. Infinite loops require Ctrl+C. |

---

*End of Plan9Basic Language Reference v0.8*
