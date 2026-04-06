# Plan9Basic

A fast, compact BASIC interpreter written in **C++23**, compiled to a stack-machine bytecode and executed by a lightweight virtual machine.
Designed for scripting, education, retro-computing experimentation, and — looking ahead — 3-D graphics pipelines.

---

## Features

| Category | What's included |
|---|---|
| **Language** | Variables (numeric + string), arrays (1-D / 2-D / **3-D**), scalars, expressions with full operator precedence |
| **Control flow** | `IF / ELSEIF / ELSE / ENDIF`, `FOR / NEXT` (any `STEP`), `WHILE / ENDWHILE`, `DO / LOOP`, `REPEAT / UNTIL`, `BREAK`, `CONTINUE`, `GOTO`, `GOSUB / RETURN`, `SELECT / CASE` |
| **Functions** | User-defined `FUNCTION / ENDFUNCTION` with parameters, `LOCAL` variables, recursion |
| **I/O** | `PRINT` / `PRINTLN`, `INPUT`, `LINE INPUT`, `INKEY$()`, `INPUT$(n)`, `PRINT USING` / `FORMAT$`, file I/O (`OPEN / CLOSE / INPUT# / PRINT# / LINE INPUT# / WRITE# / EOF()`) |
| **Strings** | Full string library: `LEN`, `MID$`, `LEFT$`, `RIGHT$`, `INSTR`, `STR$`, `VAL`, `UCASE$`, `LCASE$`, `TRIM$`, `LTRIM$`, `RTRIM$`, `CHR$`, `ASC`, `SPACE$`, `STRING$`, `SPLIT$`, `REPLACE$`, `FORMAT$` |
| **Math** | `ABS`, `SQR`, `INT`, `FIX`, `CEIL`, `FLOOR`, `SGN`, `ROUND`, `RND`, `PI`, `SIN`, `COS`, `TAN`, `ATN`, `LOG`, `LOG2`, `LOG10`, `EXP`, `POW`, `MAX`, `MIN` |
| **Bitwise** | `BAND`, `BOR`, `BXOR`, `BNOT`, `SHL`, `SHR` (logical/unsigned), `CLNG` |
| **Error handling** | `ON ERROR GOTO`, `RESUME`, `RESUME NEXT`, `ERR`, `ERL` |
| **System** | `SHELL`, `DATE$()`, `TIME$()`, `TIMER`, `CLS`, `LOCATE`, `COLOR`, `INCLUDE`, `ASSERT` |
| **Compiler** | Single-pass recursive-descent; detects use-before-assignment at compile time with source-location messages |
| **REPL** | Interactive line-by-line mode with `.run`, `.list`, `.new`, `.help`, `.quit` |

---

## 3-D Array Support

Arrays up to three dimensions are natively supported and stored as a flat contiguous block — ideal for future 3-D graphics work:

```basic
DIM cube[planes, rows, cols]   ' allocates planes×rows×cols elements
cube[p, r, c] = value          ' write with 3-index syntax
v = cube[p, r, c]              ' read with 3-index syntax
```

Flat index formula: `((p−1)×rows + (r−1))×cols + c`

**Example — fill a 2×3×4 cube:**
```basic
DIM cube[2, 3, 4]

FOR p = 1 TO 2
  FOR r = 1 TO 3
    FOR c = 1 TO 4
      cube[p, r, c] = p * 100 + r * 10 + c
    NEXT c
  NEXT r
NEXT p

PRINTLN cube[1, 2, 3]   ' 123
PRINTLN cube[2, 3, 4]   ' 234
```

> **Note:** The row and column sizes in `DIM` must be **numeric literals** (not variables) for the `[p, r, c]` syntax to work.  If you use a variable for those dimensions, only flat single-index access (`cube[i]`) is available.

---

## Building

### Requirements

| | Windows | Linux |
|---|---|---|
| Compiler | MSVC 19.50+ (VS 2026) | GCC 13+ or Clang 17+ |
| CMake | 3.28+ | 3.28+ |
| C++ standard | C++23 | C++23 |

### Windows (Visual Studio 2026)

```bat
cmake -B build_vs18 -G "Visual Studio 18 2026" -A x64
cmake --build build_vs18 --config Release
```

The executable is placed at `build_vs18\cli\Release\p9b.exe`.

Alternatively open `build_vs18\Plan9Basic.slnx` in Visual Studio and press **F7**.

### Linux / macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# executable: build/cli/p9b
```

---

## Usage

### Run a file

```bash
p9b program.bas
```

### Interactive REPL

```bash
p9b
```

REPL commands:

| Command | Action |
|---|---|
| `.run` | Execute the current buffer |
| `.list` | Show the current buffer |
| `.new` | Clear the buffer |
| `.help` | Show REPL help |
| `.quit` | Exit |
| *(empty line)* | Also executes the buffer |

### Options

```
p9b --help       Show help and version
p9b --version    Show version
```

---

## Project Structure

```
Plan9Basic/
├── cli/                  Command-line entry point (main.cpp, REPL)
├── engine/               Core interpreter
│   ├── lexer.h / .cpp    Tokeniser
│   ├── parser.h / .cpp   Recursive-descent compiler → bytecode
│   ├── exec.h / .cpp     Stack-machine virtual machine
│   ├── basic.h / .cpp    Public API (compile + execute)
│   └── p9b_types.h       Shared types, opcodes, instruction structs
├── libs/                 Built-in function libraries (header-only)
│   ├── num_lib.h         Math + bitwise functions
│   └── str_lib.h         String functions
├── tests/                Test suites
│   ├── phase2/ … phase9/ Feature tests by development phase
│   └── review/           Bug-fix regression tests (run_tests.sh)
├── docs/
│   └── Plan9Basic_Language_Reference.md  Full language reference
└── CMakeLists.txt
```

---

## Running Tests

Each test suite has a `run_tests.sh` script. Run from the project root:

```bash
# Review / regression suite
bash tests/review/run_tests.sh build/cli/p9b

# All phase suites
for d in tests/phase*/; do
  bash "$d/run_tests.sh" build/cli/p9b
done
```

On Windows, use Git Bash or WSL to run the shell scripts.

---

## Language Quick-Start

```basic
' Hello World
PRINTLN "Hello, World!"

' Variables
x = 42
name$ = "Plan9Basic"
PRINTLN name$, "version", x

' FOR loop
FOR i = 1 TO 5
  PRINT i, " "
NEXT i
PRINTLN

' Functions
FUNCTION Factorial(n)
  IF n <= 1 THEN
    Factorial = 1
  ELSE
    Factorial = n * Factorial(n - 1)
  ENDIF
ENDFUNCTION

PRINTLN Factorial(10)    ' 3628800

' 2-D array
DIM m[3, 3]
FOR r = 1 TO 3
  FOR c = 1 TO 3
    m[r, c] = r * 10 + c
  NEXT c
NEXT r
PRINTLN m[2, 3]          ' 23

' 3-D array
DIM cube[2, 3, 4]
cube[1, 2, 3] = 999
PRINTLN cube[1, 2, 3]    ' 999

' Error handling
ON ERROR GOTO handler
x = 1 / 0
PRINTLN "after div0"
GOTO done
handler:
  PRINTLN "caught ERR=" & ERR
  RESUME NEXT
done:
PRINTLN "done"
```

---

## Architecture

The interpreter uses a **single-pass recursive-descent compiler** that emits bytecode for a **stack-machine virtual machine**:

```
Source (.bas)
    │
    ▼
BasicLexer        — tokenises the source string
    │
    ▼
Parser            — recursive-descent, single-pass
    │  compiles to InstrArray (bytecode) + string pool + data section
    ▼
BasicExec         — stack-machine VM
    │  executes InstrArray; manages data stack, aux stack, call stack
    ▼
Output / side effects
```

Key design choices:
- All arrays are stored as **flat 1-based vectors**; multi-dimensional indexing is resolved entirely at **compile time** by the parser.
- Native functions (math, string, bitwise) are registered in a `FunctionsDictionary` and dispatched by signature key (e.g. `"ABS@N"`, `"MID$@SNN"`).
- Forward references (`GOTO`, `GOSUB`, function calls) are **backpatched** after the full program is compiled.
- The `src_map` parallel array maps every bytecode instruction to a `{line, col}` pair for precise runtime error messages.

---

## License

MIT — see source files for details.

---

## Roadmap

- [ ] 3-D graphics primitives (pixel buffer backed by 3-D arrays)
- [ ] `GRAPHICS` / `PLOT` / `LINE` / `CIRCLE` commands
- [ ] Variable-size multi-dim arrays (runtime stride table)
- [ ] `SAVE` / `LOAD` program persistence in REPL
- [ ] Optional line-number mode for classic BASIC compatibility
