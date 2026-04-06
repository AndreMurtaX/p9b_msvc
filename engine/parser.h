// =============================================================================
// parser.h  –  Plan9Basic: BASIC source → stack-machine bytecode compiler
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
// License : MIT
//
// Single-pass recursive-descent compiler.
// Reads tokens from BasicLexer and emits an InstrArray ready for BasicExec.
// Forward references (GOTO, GOSUB, function calls) are backpatched after the
// full program has been compiled.
// =============================================================================
#pragma once

#include "p9b_types.h"
#include "lexer.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace p9b {

// ─── Compiled program ─────────────────────────────────────────────────────────
// Handed to BasicExec after a successful compile().
struct Program {
    InstrArray               code;     // bytecode
    std::vector<std::string> strings;  // string-literal + signature pool
    std::vector<DataItem>    data;     // DATA statement items
    // Source-location map: src_map[ip] = {line, col} (1-based).
    // Parallel to code[]; populated by emit() for runtime error reporting.
    std::vector<std::pair<int,int>> src_map;
};

// =============================================================================
// Parser
// =============================================================================
class Parser {
public:
    explicit Parser(BasicLexer& lex, const FunctionsDictionary& builtins);

    // Compile the full token stream.  Returns true on success.
    bool compile();

    [[nodiscard]] Program&&          result()       && noexcept { return std::move(prog_); }
    [[nodiscard]] bool               hasError()     const noexcept { return error_;  }
    [[nodiscard]] const std::string& errorMessage() const noexcept { return errMsg_; }

private:
    BasicLexer&                 lex_;
    const FunctionsDictionary&  builtins_;
    Program                     prog_;

    // ── Variable tables ───────────────────────────────────────────────────────
    // Global slots: non-negative index.  Local slots: -(idx+1) → stored negative.
    std::unordered_map<std::string, int> gnum_, gstr_;   // global name → slot
    int gnext_ = 0, gsnext_ = 0;

    std::unordered_map<std::string, int> lnum_, lstr_;   // local  name → slot
    int lnnext_ = 0, lsnext_ = 0;
    bool in_func_ = false;

    // ── Array tables (global only; separate slot space from scalars) ──────────
    std::unordered_map<std::string, int> gnum_arr_, gstr_arr_;
    int gna_next_ = 0, gsa_next_ = 0;

    // ── Array multi-dim metadata (recorded at DIM when sizes are literals) ──────
    // 2-D [r, c]       → ncols needed.
    // 3-D [p, r, c]    → nrows (=planes×rows stride divisor) + ncols both needed.
    // Formula 2-D: (r-1)*ncols + c
    // Formula 3-D: ((p-1)*nrows + (r-1))*ncols + c
    std::unordered_map<std::string, int> arr_num_ncols_, arr_str_ncols_;
    std::unordered_map<std::string, int> arr_num_nrows_, arr_str_nrows_;

    // ── "Defined" tracking: variables confirmed written before being read ──────
    // Keys are upper-cased names.  Globals checked in main body; locals checked
    // inside functions.  Globals inside functions are allowed without check
    // (they may be assigned later in the program).
    std::unordered_set<std::string> gnum_def_, gstr_def_;     // global scalars
    std::unordered_set<std::string> lnum_def_, lstr_def_;     // local  scalars
    std::unordered_set<std::string> gnum_arr_def_, gstr_arr_def_; // DIM'd arrays

    // ── Function table ────────────────────────────────────────────────────────
    struct FuncInfo {
        int  entry      = -1;    // instruction index of InitFunc (-1 = forward ref)
        int  params     = 0;
        bool str_return = false; // true if FUNCTION name ends with '$'
    };
    std::unordered_map<std::string, FuncInfo> funcs_;

    // ── Backpatch queues ──────────────────────────────────────────────────────
    // Each entry: {instruction_index_to_patch, name}
    std::vector<std::pair<int,std::string>> fwd_calls_;   // CallNear patches
    std::vector<std::pair<int,std::string>> fwd_gotos_;   // GOTO patches
    std::vector<std::pair<int,std::string>> fwd_gosubs_;  // GOSUB patches

    // Labels defined so far: name → instruction index
    std::unordered_map<std::string, int> labels_;

    // ── SELECT / CASE nesting depth (for temp-variable naming) ───────────────
    int select_depth_ = 0;

    // ── Loop context (for BREAK / CONTINUE) ──────────────────────────────────
    struct LoopCtx {
        int               continue_ip; // where CONTINUE jumps (-1 = unknown)
        std::vector<int>  break_patches;
        std::vector<int>  cont_patches;
    };
    std::vector<LoopCtx> loops_;

    // ── Error state ───────────────────────────────────────────────────────────
    bool        error_  = false;
    std::string errMsg_;

    // ── Emit / patch helpers ──────────────────────────────────────────────────
    int  emit(AsmToken tok, int i = 0, double n = 0.0);
    int  here() const noexcept { return static_cast<int>(prog_.code.size()); }
    void patch(int idx, int target) noexcept;
    int  intern(const std::string& s);   // add to string pool, return index

    // ── Variable helpers ──────────────────────────────────────────────────────
    // Return the signed slot index to embed in Instr::i.
    int slot_num(const std::string& name);
    int slot_str(const std::string& name);

    // ── Definition-tracking helpers ───────────────────────────────────────────
    // fmt_pos: convert byte offset to "line L, col C" string.
    [[nodiscard]] std::string fmt_pos(int pos) const;

    // Mark a variable as written (defined) in the current scope.
    void mark_written_num(const std::string& name);
    void mark_written_str(const std::string& name);

    // Check that a scalar variable has been written before this read.
    // Returns false and calls set_error() if the variable is undefined.
    bool check_readable_num(const std::string& name, int pos);
    bool check_readable_str(const std::string& name, int pos);

    // Check that an array was DIM'd before this access.
    bool check_arr_dimmed_num(const std::string& name, int pos);
    bool check_arr_dimmed_str(const std::string& name, int pos);

    // ── Top-level parsing ─────────────────────────────────────────────────────
    void compile_program();
    bool compile_statement();   // returns false when a block-end token is found
    void skip_eols();
    void expect_eol();
    void set_error(const std::string& msg);

    // ── Expression grammar ────────────────────────────────────────────────────
    // Each level returns the ExprKind left on the virtual stack.
    ExprKind expr();
    ExprKind expr_or();
    ExprKind expr_and();
    ExprKind expr_not();
    ExprKind expr_compare();
    ExprKind expr_add();
    ExprKind expr_mul();
    ExprKind expr_unary();
    ExprKind expr_power();
    ExprKind expr_primary();

    // ── Statement compilers ───────────────────────────────────────────────────
    void compile_let();
    void compile_assign(BasToken id_tok, const std::string& name);
    void compile_print(bool newline);
    void compile_if();
    void compile_for();
    void compile_while();
    void compile_do();
    void compile_repeat();
    void compile_function();
    void compile_return();
    void compile_gosub();
    void compile_goto();
    void compile_input();
    void compile_data();
    void compile_read();
    void compile_local();
    void compile_break();
    void compile_continue();
    void compile_label(const std::string& name);
    void compile_dim();
    void compile_select();
    void compile_case_condition(int sel_slot, bool is_str);
    void compile_on();
    void compile_open();
    void compile_close();
    void compile_print_file(int fn);
    void compile_input_file(int fn);
    void compile_line_input();
    void compile_write_file();
    void compile_shell();
    void compile_resume();
    void compile_print_using(bool newline);
    void compile_locate();
    void compile_color();
    void compile_assert();
    void peephole_fold() noexcept;
    // Parse and consume a #n file-number token; returns the number (>= 1)
    // or -1 (and sets error) on bad syntax.
    int  parse_file_num();

    // ── Array helpers ─────────────────────────────────────────────────────────
    int      slot_arr_num(const std::string& name);
    int      slot_arr_str(const std::string& name);
    // Compile the index expression(s) + ] for an array access already started
    // (name token already consumed).  Returns the slot.
    int      compile_array_index(const std::string& name, bool is_str);
    ExprKind compile_array_read(const std::string& name, bool is_str, int src_pos);
    void     compile_array_assign(const std::string& name, bool is_str, int src_pos);

    // ── Function-call helper (used from expr_primary) ─────────────────────────
    ExprKind compile_call(const std::string& name, bool str_return);
};

} // namespace p9b
