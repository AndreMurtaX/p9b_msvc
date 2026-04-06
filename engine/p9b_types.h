// =============================================================================
// p9b_types.h  –  Plan9Basic C++ port: shared types and constants
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
// License : MIT  –  same as the original Pascal source
// =============================================================================
#pragma once

#include <string>
#include <vector>
#include <span>
#include <unordered_map>
#include <cstdint>

namespace p9b {

// ─── Memory / program limits ─────────────────────────────────────────────────
inline constexpr int    MAXINSTR      = 100'000; // max instructions per program
inline constexpr int    INITINSTRSIZE = 1'000;   // initial token-array allocation
inline constexpr int    MAXSTACK      = 16'384;  // main stack + aux stack capacity
inline constexpr int    MAXLOCALS     = 259;     // 256 args/locals + 3 internal regs
inline constexpr int    MAXVARS       = 515;     // 512 globals + 3 generic registers
inline constexpr int    DEFAULT_TIMEOUT = 30;    // execution timeout in seconds (0 = off)

inline constexpr double MAX_INTEGER_VALUE = 2'147'483'647.0; // above → float

// ─── BASIC source-level token kinds ──────────────────────────────────────────
// Mirrors TBasToken in lexer.pas  (alphabetical ordering preserved)
enum class BasToken : std::uint8_t {
    // A
    And, Ampersand, Append, As, Assert, At,
    // B
    Break, BreakPoint,
    // C
    Call, Case, CharArray, CRLF,
    Cls, Close, Colon, Color, Comma, Continue, CurlyClose, CurlyOpen,
    // D
    Data, Do, DoubleSquareClose, DoubleSquareOpen, Dump,
    // E
    Else, End, EndFor, EndFunction, EndIf, EndSelect,
    EndWhile, Equal, Erl, Err, Error,
    // F
    False, Float, For, Function,
    // G
    Goto, Greater, GreaterEqual, Gosub,
    // H
    Hash,
    // I
    Identifier, If, IndirectCallPtr, IndirectCallStr, Input,
    Integer,
    // J
    JsonNull,
    // L
    Label, Let, Line, Local, Locate, Loop, Lower, LowerEqual,
    // M
    Max, Min, Minus, Mod,
    // N
    Next, None, Not, NotEqual, Null, NumFunction,
    // O
    On, Open, Or, Output,
    // P
    Pipe, Plus, PointerArray, PointerArrayStr, PointerArrayPtr,
    PointerFunction, PointerIdentifier, Power,
    Print, PrintLn,
    // R
    Rem, Read, Repeat, Resume, Return, RoundClose, RoundOpen,
    RefreshRate, Restore,
    // S
    Select, SemiColon, Shell, Slash, SquareClose, SquareOpen, Star,
    Step, StrArray, StrFunction, StrIdentifier,
    String, Symbol,
    // T
    Then, Timer, To, Trace, TraceOff, TraceOn, True,
    // U
    Unknown, Until, Unwatch, Using,
    // W
    Watch, While, Write,
};

// ─── Tokenised BASIC instruction (output of the lexer) ───────────────────────
// Mirrors TBasInstr in lexer.pas
struct BasInstr {
    BasToken id  = BasToken::None;
    int      pos = 0;   // byte offset in source string
    int      len = 0;   // byte length of the token
    double   n   = 0.0; // numeric constant value, or string hash-code
};

// ─── Intermediate / assembly-level token kinds ───────────────────────────────
// Mirrors TAsmToken in exec.pas
enum class AsmToken : std::uint8_t {
    // A
    Add, AddCRLFS, AddS, And, Assert,
    // B
    Break, Breakpoint, CRLF,
    CallFar, CallFarP, CallFarS, CallNear,
    Cls, Comma, Comment, Continue,
    // C
    CaseEnd, CaseElse, CaseStart,
    // D
    Data, DataS, Div, DoStart, DoUntil, DoWhile, Dump,
    // E
    Else, ElseIfBody, ElseIfTest, End, EndFunction, EndIf,
    EndWhile, Eq, EqS, Err, Exit,
    // F
    Float, ForCycle, Function,
    // G
    Ge, GeS, Gt, GtS,
    // I
    Identifier, If, IndirectCall, InitFunc, Input, InputS,
    Integer, Inv,
    // J
    JsonObj, Jump,
    // L
    Label, Le, LeS, LoopEnd, LoopUntil, LoopWhile, Lt, LtS,
    // M
    Max, Min, Mod, Mul,
    // N
    Ne, NeS, Next, None, Nop, Not, Null,
    // O
    OnCallFar, OnCallFarP, OnCallFarS, OnGoto, OnGosub, Or,
    // P
    Pause, PointerFunction, PointerIdentifier,
    Pop, PopAux, PopStore, PopStorePtr, PopStoreS,
    PopnCall, PopnJump, PopnJump_CRLF, PopnJump_EndIf,
    Pow, Print, Push, PushAux, PushAuxS, PushAuxTOS,
    PushC, PushCS, PushPtr, PushPtrTag, PushS,
    // R
    Read, ReadS, RefreshRate, Repeat, RetFunction, Return, Restore,
    // S
    StrIdentifier, String, Sub, SubS, Symbol,
    // T
    To, Trace, TraceOff, TraceOn,
    // U
    Unknown, Until, Unwatch,
    // W
    Watch, While,

    // ── Array operations (C++ port extension) ────────────────────────────────
    // DimArr / DimArrS  : i = array slot; size is on the data stack
    // PushArr / PushArrS: i = array slot; 1-based index is on the data stack
    // PopStoreArr / …S  : i = array slot; stack = [ index | value(top) ]
    DimArr, DimArrS,
    PushArr, PushArrS,
    PopStoreArr, PopStoreArrS,

    // ── Phase 3 — Misc I/O ────────────────────────────────────────────────────
    // InkeyS    : push one char from stdin (non-blocking; "" if none ready)
    InkeyS,
    // InputN    : stack top = n (popped) → read n chars from stdin → push string
    InputN,
    // ShellCmd  : stack top = command string (popped) → execute via system()
    ShellCmd,

    // ── Phase 3 — WRITE #n (CSV-quoted file output) ───────────────────────────
    // WriteFile   : i=1 → pop string (write quoted), i=0 → pop numeric; n = file#
    WriteFile,
    // WriteFileSep: n = file-number — write comma separator
    WriteFileSep,
    // WriteFileCRLF: n = file-number — write newline
    WriteFileCRLF,

    // ── Phase 3 — ON ERROR / RESUME / ERR / ERL ───────────────────────────────
    // OnError   : i=0 → disable handler; i = target_ip → install handler
    OnError,
    // ResumeStmt: i=0 → retry error stmt; i=1 → resume NEXT stmt
    ResumeStmt,
    // PushErr   : push current error code (double)
    PushErr,
    // PushErl   : push line number of last error (double)
    PushErl,

    // ── Phase 3 — PRINT USING ─────────────────────────────────────────────────
    // PrintUsing   : i=1→string value i=0→numeric; pops value, peeks/updates
    //                format string below it on stack; writes formatted output
    PrintUsing,
    // PrintUsingEnd: pops format string, writes any trailing literals + newline
    //                i=1 → emit newline; i=0 → no newline (trailing semicolon)
    PrintUsingEnd,

    // ── Phase 6 — Interactive ─────────────────────────────────────────────────
    // LocateXY  : pop col, pop row → emit ANSI escape to position cursor
    // ColorSet  : pop bg, pop fg → emit ANSI color escape; i=0 → no bg (bg=-1 sentinel)
    // PushTimer : push elapsed seconds since program start as double
    LocateXY,
    ColorSet,
    PushTimer,

    // ── File I/O (Phase 2) ────────────────────────────────────────────────────
    // OpenFile    : i = mode (0=INPUT 1=OUTPUT 2=APPEND); n = file-number (1-based)
    //               stack top = filename string (popped)
    OpenFile,
    // CloseFile   : i = 0 → close specific file (n = file-number)
    //               i = 1 → close all open files
    CloseFile,
    // PrintFile   : i = 1 → pop string; i = 0 → pop number; n = file-number
    PrintFile,
    // PrintFileSep: n = file-number — write TAB field separator
    PrintFileSep,
    // PrintFileCRLF: n = file-number — write newline
    PrintFileCRLF,
    // InputFile   : i = numeric variable slot; n = file-number — read line → double
    InputFile,
    // InputFileS  : i = string variable slot; n = file-number — read line → string
    InputFileS,
    // LineInputFile: i = string variable slot; n = file-number — read whole line
    LineInputFile,
    // EofFile     : stack top = file-number (popped) → push -1.0 (EOF) or 0.0
    EofFile,
};

// ─── Runtime data cell ───────────────────────────────────────────────────────
// Mirrors TAsmData in exec.pas.
// double instead of Extended: identical on both MSVC and GCC/Linux x64.
struct AsmData {
    double      n = 0.0;
    void*       p = nullptr;
    std::string s;
};

// ─── Expression type tag ─────────────────────────────────────────────────────
// Mirrors TExprKind in exec.pas
enum class ExprKind : std::uint8_t { Number, Pointer, String };

// ─── Tokenised intermediate-code instruction (used by the ASM lexer) ─────────
// Mirrors TStringToken / TStringTokens in exec.pas
struct StringToken {
    std::string str;
    AsmToken    token = AsmToken::None;
};
using StringTokens = std::vector<StringToken>;

// ─── Bound function types ─────────────────────────────────────────────────────
// A native function importable into the BASIC engine.
//
// Using a plain function pointer (not std::function) keeps LinkFunction
// a trivial struct: no heap allocation per registration, O(1) call dispatch.
// Lambdas without captures decay to function pointers via unary +.
//
// Mirrors TBindFunction / TLinkFunction in exec.pas
using BindFunction = AsmData(*)(std::span<AsmData>);

struct LinkFunction {
    bool         farCall = false;   // true = native import, false = user-defined
    BindFunction entry   = nullptr;
};

// Key = function signature string, e.g. "ABS@N", "MID$@SNN"
using FunctionsDictionary = std::unordered_map<std::string, LinkFunction>;

// ─── Stack-machine instruction (loaded into TExec) ───────────────────────────
// Mirrors TInstr in exec.pas.
// 'proc' is resolved at load-time into a dispatch index; replaced here by
// the token value used in Exec's dispatch table.
struct Instr {
    AsmToken token = AsmToken::None;
    int      i     = 0;      // variable index or string-pool offset
    double   n     = 0.0;    // inline numeric constant
};
using InstrArray = std::vector<Instr>;

// ─── DATA/READ statement item ─────────────────────────────────────────────────
// Mirrors TDataItem in exec.pas
struct DataItem {
    char dataType = 'n';  // 'n' = numeric  |  '$' = string
    int  dataPos  = 0;    // instruction index in the ASM stream
};

// ─── Stack-machine execution status ──────────────────────────────────────────
enum class ExecStatus : std::uint8_t { Idle, Running };

// ─── Runtime error codes ──────────────────────────────────────────────────────
enum class RTError : std::uint8_t {
    StackOverflow,
    StackUnderflow,
    StackTypeMismatch,
    InvalidParams,
    DimIndexBound,
    PrintStackOverflow,
    PrintSyntaxMismatch,
    AuxStackTypeMismatch,
    AuxStackOverflow,
    AuxStackUnderflow,
    DivisionByZero,
    StringSize,
    UnknownInstr,
    UserMessage,
};

} // namespace p9b
