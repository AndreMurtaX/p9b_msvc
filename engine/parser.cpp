// =============================================================================
// parser.cpp  –  Plan9Basic: BASIC → bytecode compiler
// =============================================================================

#include "parser.h"
#include "p9b_utils.h"

#include <cmath>
#include <format>

namespace p9b {

// =============================================================================
// Constructor
// =============================================================================
Parser::Parser(BasicLexer& lex, const FunctionsDictionary& builtins)
    : lex_(lex), builtins_(builtins) {}

// =============================================================================
// Public entry point
// =============================================================================
bool Parser::compile() {
    compile_program();
    if (error_) return false;

    // ── Backpatch GOTO targets ────────────────────────────────────────────────
    // Entries with "__resume__" prefix patch .n (for RESUME label: target).
    static const std::string RESUME_PFX = "__resume__";
    for (auto& [idx, name] : fwd_gotos_) {
        if (name.substr(0, RESUME_PFX.size()) == RESUME_PFX) {
            const std::string lbl = name.substr(RESUME_PFX.size());
            auto it = labels_.find(lbl);
            if (it == labels_.end()) { set_error("Undefined label: " + lbl); break; }
            prog_.code[static_cast<std::size_t>(idx)].n =
                static_cast<double>(it->second);
        } else {
            auto it = labels_.find(name);
            if (it == labels_.end()) { set_error("Undefined label: " + name); break; }
            patch(idx, it->second);
        }
    }
    // ── Backpatch GOSUB targets ───────────────────────────────────────────────
    for (auto& [idx, name] : fwd_gosubs_) {
        auto it = labels_.find(name);
        if (it == labels_.end()) { set_error("Undefined label: " + name); break; }
        patch(idx, it->second);
    }
    // ── Backpatch forward function calls ─────────────────────────────────────
    for (auto& [idx, name] : fwd_calls_) {
        auto it = funcs_.find(name);
        if (it == funcs_.end() || it->second.entry < 0) {
            set_error("Undefined function: " + name); break;
        }
        patch(idx, it->second.entry);
    }

    if (!error_) emit(AsmToken::End);
    return !error_;
}

// =============================================================================
// Emit / patch / pool
// =============================================================================
int Parser::emit(AsmToken tok, int i, double n) {
    prog_.code.push_back({ tok, i, n });
    prog_.src_map.push_back(lex_.pos_to_line_col(lex_.currPos()));
    return here() - 1;
}

void Parser::patch(int idx, int target) noexcept {
    prog_.code[static_cast<std::size_t>(idx)].i = target;
}

int Parser::intern(const std::string& s) {
    prog_.strings.push_back(s);
    return static_cast<int>(prog_.strings.size()) - 1;
}

// =============================================================================
// Variable slot helpers
// Negative return value → local variable (stored as -(idx+1)).
// Non-negative → global variable.
// =============================================================================
int Parser::slot_num(const std::string& name) {
    // Always normalise to uppercase so that  x, X, and COUNTER
    // all resolve to the same runtime slot — BASIC is case-insensitive.
    const std::string key = utils::upper(name);
    if (in_func_) {
        // 1. Already a known local → reuse local slot.
        auto lit = lnum_.find(key);
        if (lit != lnum_.end()) return -(lit->second + 1);
        // 2. Already a known global → use global slot (allows functions to read
        //    and write global variables without shadowing them).
        auto git = gnum_.find(key);
        if (git != gnum_.end()) return git->second;
        // 3. Brand-new name inside a function → create a new local slot.
        const int idx = lnnext_++;
        lnum_.emplace(key, idx);
        return -(idx + 1);
    }
    auto [it, ins] = gnum_.emplace(key, gnext_);
    if (ins) ++gnext_;
    return it->second;
}

int Parser::slot_str(const std::string& name) {
    // Always normalise to uppercase — see slot_num().
    const std::string key = utils::upper(name);
    if (in_func_) {
        // 1. Already a known local → reuse local slot.
        auto lit = lstr_.find(key);
        if (lit != lstr_.end()) return -(lit->second + 1);
        // 2. Already a known global → use global slot.
        auto git = gstr_.find(key);
        if (git != gstr_.end()) return git->second;
        // 3. Brand-new name inside a function → create a new local slot.
        const int idx = lsnext_++;
        lstr_.emplace(key, idx);
        return -(idx + 1);
    }
    auto [it, ins] = gstr_.emplace(key, gsnext_);
    if (ins) ++gsnext_;
    return it->second;
}

// =============================================================================
// Array slot helpers  (separate slot space from scalars)
// =============================================================================
int Parser::slot_arr_num(const std::string& name) {
    const std::string key = utils::upper(name);
    auto [it, ins] = gnum_arr_.emplace(key, gna_next_);
    if (ins) ++gna_next_;
    return it->second;
}

int Parser::slot_arr_str(const std::string& name) {
    const std::string key = utils::upper(name);
    auto [it, ins] = gstr_arr_.emplace(key, gsa_next_);
    if (ins) ++gsa_next_;
    return it->second;
}

// =============================================================================
// DIM statement:  DIM arr[size]  |  DIM arr[rows,cols]  |  DIM arr[p,r,c]
// Also supports multiple declarations:  DIM a[10], b$[20], c[2,3,4]
// =============================================================================
void Parser::compile_dim() {
    while (!error_ &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null  &&
           lex_.currTok() != BasToken::Colon)
    {
        const bool is_str = (lex_.currTok() == BasToken::StrArray);
        if (lex_.currTok() != BasToken::Unknown && !is_str) {
            set_error("Expected array name after DIM");
            return;
        }
        const std::string name = lex_.currS();
        lex_.advance();

        // Parse size expression(s): 1-D, 2-D, or 3-D.
        expr();  // first dimension: size (1-D) | rows (2-D) | planes (3-D)

        if (lex_.currTok() == BasToken::Comma) {
            lex_.advance();
            expr();  // second dimension: cols (2-D) | rows (3-D)

            if (lex_.currTok() == BasToken::Comma) {
                // ── 3-D:  DIM arr[planes, rows, cols] ────────────────────────
                lex_.advance();

                // Record nrows (second dim) when it is a compile-time literal.
                {
                    const int sz = static_cast<int>(prog_.code.size());
                    if (sz > 0 &&
                        prog_.code[static_cast<std::size_t>(sz - 1)].token == AsmToken::PushC)
                    {
                        const int nrows = static_cast<int>(
                            prog_.code[static_cast<std::size_t>(sz - 1)].n);
                        if (nrows > 0) {
                            const auto key = utils::upper(name);
                            if (is_str) arr_str_nrows_[key] = nrows;
                            else        arr_num_nrows_[key] = nrows;
                        }
                    }
                }
                emit(AsmToken::Mul);  // planes * rows on stack

                expr();  // third dimension (cols)

                // Record ncols (third dim) when it is a compile-time literal.
                {
                    const int sz = static_cast<int>(prog_.code.size());
                    if (sz > 0 &&
                        prog_.code[static_cast<std::size_t>(sz - 1)].token == AsmToken::PushC)
                    {
                        const int ncols = static_cast<int>(
                            prog_.code[static_cast<std::size_t>(sz - 1)].n);
                        if (ncols > 0) {
                            const auto key = utils::upper(name);
                            if (is_str) arr_str_ncols_[key] = ncols;
                            else        arr_num_ncols_[key] = ncols;
                        }
                    }
                }
                emit(AsmToken::Mul);  // (planes * rows) * cols = total elements

            } else {
                // ── 2-D:  DIM arr[rows, cols] ────────────────────────────────
                // Record ncols (second dim) when it is a compile-time literal.
                {
                    const int sz = static_cast<int>(prog_.code.size());
                    if (sz > 0 &&
                        prog_.code[static_cast<std::size_t>(sz - 1)].token == AsmToken::PushC)
                    {
                        const int ncols = static_cast<int>(
                            prog_.code[static_cast<std::size_t>(sz - 1)].n);
                        if (ncols > 0) {
                            const auto key = utils::upper(name);
                            if (is_str) arr_str_ncols_[key] = ncols;
                            else        arr_num_ncols_[key] = ncols;
                        }
                    }
                }
                emit(AsmToken::Mul);  // rows * cols = total elements
            }
        }

        if (lex_.currTok() == BasToken::SquareClose) lex_.advance();

        if (is_str) {
            gstr_arr_def_.insert(utils::upper(name));
            emit(AsmToken::DimArrS, slot_arr_str(name));
        } else {
            gnum_arr_def_.insert(utils::upper(name));
            emit(AsmToken::DimArr,  slot_arr_num(name));
        }

        // Allow comma-separated list:  DIM a[10], b$[5]
        if (lex_.currTok() == BasToken::Comma) lex_.advance();
    }
    expect_eol();
}

// =============================================================================
// Array index helper — compiles the index expression(s) inside [ … ]
// The opening '[' was already consumed by the lexer as part of the array token.
// Handles 1-D arr[i], 2-D arr[r,c], and 3-D arr[p,r,c].
// All multi-dim accesses are flattened to a single 1-based flat index at
// compile time; no new opcodes are needed.
//   2-D formula: (row-1)*ncols + col
//   3-D formula: ((p-1)*nrows + (r-1))*ncols + c
// After this call the closing ']' has been consumed.
// Returns the number of dimensions (1, 2, or 3).
// For 2-D/3-D, dimension sizes other than the first must be literal constants
// at DIM time; otherwise the multi-index syntax produces a compile error.
// =============================================================================
int Parser::compile_array_index(const std::string& name, bool is_str) {
    // Strategy: parse each index expression into prog_.code, note the code
    // boundaries, then — once we know the dimension count — cut the sub-arrays
    // out and re-emit them interleaved with the flattening formula.
    // This keeps the parser single-pass while correctly handling 1-D, 2-D, 3-D.
    // Both prog_.code and prog_.src_map are kept in sync throughout.

    const auto  key       = utils::upper(name);
    const auto& ncols_map = is_str ? arr_str_ncols_ : arr_num_ncols_;
    const auto& nrows_map = is_str ? arr_str_nrows_ : arr_num_nrows_;

    // ── Index 1 ───────────────────────────────────────────────────────────────
    const auto base1 = static_cast<int>(prog_.code.size());
    expr();

    if (lex_.currTok() != BasToken::Comma) {
        // 1-D: flat index already on the stack — nothing to rewrite.
        if (lex_.currTok() == BasToken::SquareClose) lex_.advance();
        return 1;
    }
    lex_.advance();  // consume first ','

    // ── Index 2 ───────────────────────────────────────────────────────────────
    const auto base2 = static_cast<int>(prog_.code.size());
    expr();

    if (lex_.currTok() != BasToken::Comma) {
        // 2-D: arr[row, col]
        // Formula: (row - 1) * ncols + col
        auto it = ncols_map.find(key);
        if (it == ncols_map.end()) {
            set_error("2D array '" + name +
                      "': column count must be a literal constant in the DIM statement");
            if (lex_.currTok() == BasToken::SquareClose) lex_.advance();
            return 2;
        }
        const int ncols = it->second;

        // Cut idx1 and idx2 code segments, then re-emit with formula.
        std::vector<Instr>              c1(prog_.code.begin()    + base1,
                                           prog_.code.begin()    + base2);
        std::vector<std::pair<int,int>> s1(prog_.src_map.begin() + base1,
                                           prog_.src_map.begin() + base2);
        std::vector<Instr>              c2(prog_.code.begin()    + base2,
                                           prog_.code.end());
        std::vector<std::pair<int,int>> s2(prog_.src_map.begin() + base2,
                                           prog_.src_map.end());
        prog_.code.resize(   static_cast<std::size_t>(base1));
        prog_.src_map.resize(static_cast<std::size_t>(base1));

        // row
        prog_.code.insert(prog_.code.end(),       c1.begin(), c1.end());
        prog_.src_map.insert(prog_.src_map.end(), s1.begin(), s1.end());
        emit(AsmToken::PushC, 0, 1.0);
        emit(AsmToken::Sub);                                    // row - 1
        emit(AsmToken::PushC, 0, static_cast<double>(ncols));
        emit(AsmToken::Mul);                                    // (row-1)*ncols
        // col
        prog_.code.insert(prog_.code.end(),       c2.begin(), c2.end());
        prog_.src_map.insert(prog_.src_map.end(), s2.begin(), s2.end());
        emit(AsmToken::Add);                                    // (row-1)*ncols + col

        if (lex_.currTok() == BasToken::SquareClose) lex_.advance();
        return 2;
    }
    lex_.advance();  // consume second ','

    // ── Index 3 ───────────────────────────────────────────────────────────────
    const auto base3 = static_cast<int>(prog_.code.size());
    expr();

    // 3-D: arr[p, r, c]
    // Formula: ((p-1)*nrows + (r-1))*ncols + c
    auto it_c = ncols_map.find(key);
    auto it_r = nrows_map.find(key);
    if (it_c == ncols_map.end() || it_r == nrows_map.end()) {
        set_error("3D array '" + name +
                  "': row and column counts must be literal constants in the DIM statement");
        if (lex_.currTok() == BasToken::SquareClose) lex_.advance();
        return 3;
    }
    const int ncols = it_c->second;
    const int nrows = it_r->second;

    // Cut idx1, idx2, idx3 code segments, then re-emit with formula.
    std::vector<Instr>              c1(prog_.code.begin()    + base1,
                                       prog_.code.begin()    + base2);
    std::vector<std::pair<int,int>> s1(prog_.src_map.begin() + base1,
                                       prog_.src_map.begin() + base2);
    std::vector<Instr>              c2(prog_.code.begin()    + base2,
                                       prog_.code.begin()    + base3);
    std::vector<std::pair<int,int>> s2(prog_.src_map.begin() + base2,
                                       prog_.src_map.begin() + base3);
    std::vector<Instr>              c3(prog_.code.begin()    + base3,
                                       prog_.code.end());
    std::vector<std::pair<int,int>> s3(prog_.src_map.begin() + base3,
                                       prog_.src_map.end());
    prog_.code.resize(   static_cast<std::size_t>(base1));
    prog_.src_map.resize(static_cast<std::size_t>(base1));

    // p
    prog_.code.insert(prog_.code.end(),       c1.begin(), c1.end());
    prog_.src_map.insert(prog_.src_map.end(), s1.begin(), s1.end());
    emit(AsmToken::PushC, 0, 1.0);
    emit(AsmToken::Sub);                                        // p - 1
    emit(AsmToken::PushC, 0, static_cast<double>(nrows));
    emit(AsmToken::Mul);                                        // (p-1)*nrows
    // r
    prog_.code.insert(prog_.code.end(),       c2.begin(), c2.end());
    prog_.src_map.insert(prog_.src_map.end(), s2.begin(), s2.end());
    emit(AsmToken::PushC, 0, 1.0);
    emit(AsmToken::Sub);                                        // r - 1
    emit(AsmToken::Add);                                        // (p-1)*nrows + (r-1)
    emit(AsmToken::PushC, 0, static_cast<double>(ncols));
    emit(AsmToken::Mul);                                        // × ncols
    // c
    prog_.code.insert(prog_.code.end(),       c3.begin(), c3.end());
    prog_.src_map.insert(prog_.src_map.end(), s3.begin(), s3.end());
    emit(AsmToken::Add);                                        // flat 1-based index

    if (lex_.currTok() == BasToken::SquareClose) lex_.advance();
    return 3;
}

// =============================================================================
// Array element read  —  called from expr_primary after the name token is consumed
// Current token is the start of the index expression ([ was absorbed by lexer).
// =============================================================================
ExprKind Parser::compile_array_read(const std::string& name, bool is_str, int src_pos) {
    // Check that the array was DIM'd before this read.
    if (is_str) {
        if (!check_arr_dimmed_str(name, src_pos)) return ExprKind::String;
    } else {
        if (!check_arr_dimmed_num(name, src_pos)) return ExprKind::Number;
    }

    compile_array_index(name, is_str);

    if (is_str) {
        emit(AsmToken::PushArrS, slot_arr_str(name));
        return ExprKind::String;
    } else {
        emit(AsmToken::PushArr, slot_arr_num(name));
        return ExprKind::Number;
    }
}

// =============================================================================
// Array element assignment  —  called from compile_statement after name consumed
// Pattern:  arr[i] = expr   or   arr[r, c] = expr
// =============================================================================
void Parser::compile_array_assign(const std::string& name, bool is_str, int src_pos) {
    // Check that the array was DIM'd before this write.
    if (is_str) { if (!check_arr_dimmed_str(name, src_pos)) return; }
    else         { if (!check_arr_dimmed_num(name, src_pos)) return; }

    // Compile index expression; [ already consumed by lexer as part of token.
    compile_array_index(name, is_str);  // flat index → will be below value on stack

    if (lex_.currTok() != BasToken::Equal) {
        set_error("Expected '=' in array element assignment");
        return;
    }
    lex_.advance();

    expr();  // value → on top of stack

    if (is_str)
        emit(AsmToken::PopStoreArrS, slot_arr_str(name));
    else
        emit(AsmToken::PopStoreArr,  slot_arr_num(name));

    expect_eol();
}

// =============================================================================
// Error helpers
// =============================================================================
void Parser::set_error(const std::string& msg) {
    if (!error_) { error_ = true; errMsg_ = msg; }
}

// =============================================================================
// Definition-tracking helpers
// =============================================================================

// Convert a byte offset in the source to "line L, col C" (1-based).
std::string Parser::fmt_pos(int pos) const {
    auto [line, col] = lex_.pos_to_line_col(pos);
    return "[line " + std::to_string(line) + ", col " + std::to_string(col) + "]";
}

// Mark a scalar variable as "written" in the current scope.
void Parser::mark_written_num(const std::string& name) {
    const auto key = utils::upper(name);
    if (in_func_) lnum_def_.insert(key);
    else           gnum_def_.insert(key);
}
void Parser::mark_written_str(const std::string& name) {
    const auto key = utils::upper(name);
    if (in_func_) lstr_def_.insert(key);
    else           gstr_def_.insert(key);
}

// Check that a scalar numeric variable has been written before being read.
// Inside a function: locals are checked; globals are trusted (may be set
// after the function definition — checked in the main body instead).
bool Parser::check_readable_num(const std::string& name, int pos) {
    const auto key = utils::upper(name);
    if (in_func_) {
        // slot_num() now uses uppercase keys, so lnum_ is keyed by 'key'.
        if (lnum_.count(key)) {
            if (!lnum_def_.count(key)) {
                set_error("local variable '" + name +
                          "' used before assignment " + fmt_pos(pos));
                return false;
            }
        }
        // Otherwise it resolves to a global → allowed inside functions.
        return true;
    }
    if (!gnum_def_.count(key)) {
        set_error("variable '" + name +
                  "' used before assignment " + fmt_pos(pos));
        return false;
    }
    return true;
}

bool Parser::check_readable_str(const std::string& name, int pos) {
    const auto key = utils::upper(name);
    if (in_func_) {
        // slot_str() now uses uppercase keys, so lstr_ is keyed by 'key'.
        if (lstr_.count(key)) {
            if (!lstr_def_.count(key)) {
                set_error("local variable '" + name +
                          "' used before assignment " + fmt_pos(pos));
                return false;
            }
        }
        return true;
    }
    if (!gstr_def_.count(key)) {
        set_error("variable '" + name +
                  "' used before assignment " + fmt_pos(pos));
        return false;
    }
    return true;
}

// Check that an array was DIM'd before this read/write.
bool Parser::check_arr_dimmed_num(const std::string& name, int pos) {
    if (!gnum_arr_def_.count(utils::upper(name))) {
        set_error("array '" + name + "' accessed before DIM " + fmt_pos(pos));
        return false;
    }
    return true;
}
bool Parser::check_arr_dimmed_str(const std::string& name, int pos) {
    if (!gstr_arr_def_.count(utils::upper(name))) {
        set_error("array '" + name + "' accessed before DIM " + fmt_pos(pos));
        return false;
    }
    return true;
}

void Parser::skip_eols() {
    while (lex_.currTok() == BasToken::CRLF)
        lex_.advance();
}

void Parser::expect_eol() {
    // Allow colon-separated statements; stop at ELSE so that single-line
    // "IF … THEN stmt ELSE stmt" can still find the ELSE after the THEN body.
    while (lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null  &&
           lex_.currTok() != BasToken::Colon &&
           lex_.currTok() != BasToken::Else)
        lex_.advance();
    if (lex_.currTok() == BasToken::CRLF || lex_.currTok() == BasToken::Colon)
        lex_.advance();
    // ELSE is intentionally left for the caller (compile_if single-line handler)
}

// =============================================================================
// Top-level program compilation
// =============================================================================
void Parser::compile_program() {
    lex_.gotoToken(1);  // skip sentinel at index 0
    skip_eols();
    while (!error_ && lex_.currTok() != BasToken::Null)
        compile_statement();
}

// Returns false if a block-ending token was reached (ENDIF etc.) — caller
// handles those tokens, not this function.
bool Parser::compile_statement() {
    skip_eols();
    if (error_) return false;

    const BasToken tok = lex_.currTok();

    switch (tok) {
        case BasToken::Null:        return false;
        case BasToken::End: {
            // "END X" where X is a block keyword → don't consume; let the
            // enclosing block handler recognise the two-token terminator.
            const BasToken nxt = lex_.nextTok();
            if (nxt == BasToken::If       || nxt == BasToken::While    ||
                nxt == BasToken::Function || nxt == BasToken::For      ||
                nxt == BasToken::Select)
                return false;
            lex_.advance(); emit(AsmToken::End);
            return true;
        }
        case BasToken::Cls:         lex_.advance(); emit(AsmToken::Cls);    return true;
        case BasToken::Dump:        lex_.advance(); emit(AsmToken::Dump);   return true;
        case BasToken::TraceOn:     lex_.advance(); emit(AsmToken::TraceOn);  return true;
        case BasToken::TraceOff:    lex_.advance(); emit(AsmToken::TraceOff); return true;
        case BasToken::Restore:     lex_.advance(); emit(AsmToken::Restore);  return true;

        case BasToken::Let:         lex_.advance(); compile_let();          return true;
        case BasToken::Print:
            lex_.advance();
            if (lex_.currTok() == BasToken::Hash) {
                const int fn = parse_file_num();
                if (!error_) compile_print_file(fn);
            } else if (lex_.currTok() == BasToken::Using) {
                lex_.advance();
                compile_print_using(true);
            } else {
                compile_print(false);
            }
            return true;
        case BasToken::PrintLn:     lex_.advance(); compile_print(true);    return true;
        case BasToken::If:          lex_.advance(); compile_if();           return true;
        case BasToken::For:         lex_.advance(); compile_for();          return true;
        case BasToken::While:       lex_.advance(); compile_while();        return true;
        case BasToken::Do:          lex_.advance(); compile_do();           return true;
        case BasToken::Repeat:      lex_.advance(); compile_repeat();       return true;
        case BasToken::Function:    lex_.advance(); compile_function();     return true;
        case BasToken::Return:      lex_.advance(); compile_return();       return true;
        case BasToken::Gosub:       lex_.advance(); compile_gosub();        return true;
        case BasToken::Goto:        lex_.advance(); compile_goto();         return true;
        case BasToken::Input:
            lex_.advance();
            if (lex_.currTok() == BasToken::Hash) {
                const int fn = parse_file_num();
                if (!error_) compile_input_file(fn);
            } else {
                compile_input();
            }
            return true;
        case BasToken::Data:        lex_.advance(); compile_data();         return true;
        case BasToken::Read:        lex_.advance(); compile_read();         return true;
        case BasToken::Local:       lex_.advance(); compile_local();        return true;
        case BasToken::Break:       lex_.advance(); compile_break();        return true;
        case BasToken::Continue:    lex_.advance(); compile_continue();     return true;
        case BasToken::Select:      lex_.advance(); compile_select();       return true;
        case BasToken::On:          lex_.advance(); compile_on();           return true;
        case BasToken::Open:        lex_.advance(); compile_open();         return true;
        case BasToken::Close:       lex_.advance(); compile_close();        return true;
        case BasToken::Line:        lex_.advance(); compile_line_input();   return true;
        case BasToken::Write:       lex_.advance(); compile_write_file();   return true;
        case BasToken::Shell:       lex_.advance(); compile_shell();        return true;
        case BasToken::Resume:      lex_.advance(); compile_resume();       return true;
        case BasToken::Locate:      lex_.advance(); compile_locate();       return true;
        case BasToken::Color:       lex_.advance(); compile_color();        return true;
        case BasToken::Assert:      lex_.advance(); compile_assert();       return true;

        // Block-end tokens — caller's responsibility
        case BasToken::EndIf:
        case BasToken::Else:
        case BasToken::EndWhile:
        case BasToken::Next:
        case BasToken::Loop:
        case BasToken::Until:
        case BasToken::EndFunction:
        case BasToken::EndFor:
        case BasToken::EndSelect:
            return false;

        // Label definition (ends with ':')
        case BasToken::Label: {
            std::string name = lex_.currS();
            // Strip trailing ':'
            if (!name.empty() && name.back() == ':') name.pop_back();
            lex_.advance();
            compile_label(name);
            return true;
        }

        // Implicit assignment: x = … or x$ = …
        // Also intercepts the DIM keyword (lexer sees it as plain Identifier)
        case BasToken::Identifier:
        case BasToken::StrIdentifier:
        case BasToken::PointerIdentifier: {
            const std::string name = lex_.currS();
            lex_.advance();
            if (tok == BasToken::Identifier && utils::upper(name) == "DIM") {
                compile_dim();
                return true;
            }
            compile_assign(tok, name);
            return true;
        }

        // Numeric array element assignment:  arr[i] = expr
        case BasToken::Unknown: {
            const int         src_pos = lex_.currPos();
            const std::string name    = lex_.currS();
            lex_.advance();
            compile_array_assign(name, false, src_pos);
            return true;
        }

        // String array element assignment:  arr$[i] = expr
        case BasToken::StrArray: {
            const int         src_pos = lex_.currPos();
            const std::string name    = lex_.currS();
            lex_.advance();
            compile_array_assign(name, true, src_pos);
            return true;
        }

        default:
            // Skip unrecognised token silently
            lex_.advance();
            return true;
    }
}

// =============================================================================
// Expression parser  (recursive descent, returns ExprKind on the virtual stack)
// =============================================================================
ExprKind Parser::expr() { return expr_or(); }

ExprKind Parser::expr_or() {
    auto k = expr_and();
    while (!error_ && lex_.currTok() == BasToken::Or) {
        lex_.advance();
        expr_and();
        emit(AsmToken::Or);
        k = ExprKind::Number;
    }
    return k;
}

ExprKind Parser::expr_and() {
    auto k = expr_not();
    while (!error_ && lex_.currTok() == BasToken::And) {
        lex_.advance();
        expr_not();
        emit(AsmToken::And);
        k = ExprKind::Number;
    }
    return k;
}

ExprKind Parser::expr_not() {
    if (lex_.currTok() == BasToken::Not) {
        lex_.advance();
        expr_not();
        emit(AsmToken::Not);
        return ExprKind::Number;
    }
    return expr_compare();
}

ExprKind Parser::expr_compare() {
    auto k = expr_add();
    while (!error_) {
        const BasToken op = lex_.currTok();
        if (op != BasToken::Equal      && op != BasToken::NotEqual  &&
            op != BasToken::Lower      && op != BasToken::LowerEqual &&
            op != BasToken::Greater    && op != BasToken::GreaterEqual)
            break;
        lex_.advance();
        const auto rk   = expr_add();
        const bool isstr = (k == ExprKind::String || rk == ExprKind::String);
        switch (op) {
            case BasToken::Equal:        emit(isstr ? AsmToken::EqS : AsmToken::Eq);  break;
            case BasToken::NotEqual:     emit(isstr ? AsmToken::NeS : AsmToken::Ne);  break;
            case BasToken::Lower:        emit(isstr ? AsmToken::LtS : AsmToken::Lt);  break;
            case BasToken::LowerEqual:   emit(isstr ? AsmToken::LeS : AsmToken::Le);  break;
            case BasToken::Greater:      emit(isstr ? AsmToken::GtS : AsmToken::Gt);  break;
            case BasToken::GreaterEqual: emit(isstr ? AsmToken::GeS : AsmToken::Ge);  break;
            default: break;
        }
        k = ExprKind::Number;
    }
    return k;
}

ExprKind Parser::expr_add() {
    auto k = expr_mul();
    while (!error_) {
        const BasToken op = lex_.currTok();
        if (op != BasToken::Plus && op != BasToken::Minus && op != BasToken::Ampersand) break;
        lex_.advance();
        const auto rk = expr_mul();
        if (op == BasToken::Ampersand) {
            // & is always string concatenation
            emit(AsmToken::AddS); k = ExprKind::String;
        } else if (op == BasToken::Plus) {
            if (k == ExprKind::String || rk == ExprKind::String) {
                emit(AsmToken::AddS); k = ExprKind::String;
            } else {
                emit(AsmToken::Add); peephole_fold();
            }
        } else {
            emit(AsmToken::Sub); peephole_fold(); k = ExprKind::Number;
        }
    }
    return k;
}

ExprKind Parser::expr_mul() {
    auto k = expr_unary();
    while (!error_) {
        const BasToken op = lex_.currTok();
        if (op != BasToken::Star && op != BasToken::Slash && op != BasToken::Mod) break;
        lex_.advance();
        expr_unary();
        switch (op) {
            case BasToken::Star:  emit(AsmToken::Mul); peephole_fold(); break;
            case BasToken::Slash: emit(AsmToken::Div); peephole_fold(); break;
            case BasToken::Mod:   emit(AsmToken::Mod); peephole_fold(); break;
            default: break;
        }
        k = ExprKind::Number;
    }
    return k;
}

ExprKind Parser::expr_unary() {
    if (lex_.currTok() == BasToken::Minus) {
        lex_.advance(); expr_power(); emit(AsmToken::Inv); peephole_fold(); return ExprKind::Number;
    }
    if (lex_.currTok() == BasToken::Plus) lex_.advance();
    return expr_power();
}

ExprKind Parser::expr_power() {
    auto k = expr_primary();
    if (!error_ && lex_.currTok() == BasToken::Power) {
        lex_.advance();
        expr_unary();   // right-associative
        emit(AsmToken::Pow); peephole_fold();
        k = ExprKind::Number;
    }
    return k;
}

ExprKind Parser::expr_primary() {
    if (error_) return ExprKind::Number;
    const BasToken tok = lex_.currTok();

    switch (tok) {
        // ── Numeric literals ──────────────────────────────────────────────────
        case BasToken::Integer:
        case BasToken::Float: {
            const double v = lex_.currN();
            lex_.advance();
            emit(AsmToken::PushC, 0, v);
            return ExprKind::Number;
        }
        // ── String literal ────────────────────────────────────────────────────
        case BasToken::String: {
            const int idx = intern(lex_.currS());
            lex_.advance();
            emit(AsmToken::PushCS, idx);
            return ExprKind::String;
        }
        // ── Boolean literals ──────────────────────────────────────────────────
        case BasToken::True:  lex_.advance(); emit(AsmToken::PushC, 0, 1.0); return ExprKind::Number;
        case BasToken::False: lex_.advance(); emit(AsmToken::PushC, 0, 0.0); return ExprKind::Number;

        // ── ERR / ERL / TIMER pseudo-variables ───────────────────────────────
        case BasToken::Err:   lex_.advance(); emit(AsmToken::PushErr);   return ExprKind::Number;
        case BasToken::Erl:   lex_.advance(); emit(AsmToken::PushErl);   return ExprKind::Number;
        case BasToken::Timer: lex_.advance(); emit(AsmToken::PushTimer); return ExprKind::Number;

        // ── Parenthesised expression ──────────────────────────────────────────
        case BasToken::RoundOpen: {
            lex_.advance();
            auto k = expr();
            if (lex_.currTok() == BasToken::RoundClose) lex_.advance();
            else set_error("Expected ')'");
            return k;
        }
        // ── Numeric variable ──────────────────────────────────────────────────
        case BasToken::Identifier: {
            const int         pos  = lex_.currPos();
            const std::string name = lex_.currS();
            lex_.advance();
            if (!check_readable_num(name, pos)) return ExprKind::Number;
            emit(AsmToken::Push, slot_num(name));
            return ExprKind::Number;
        }
        // ── String variable ───────────────────────────────────────────────────
        case BasToken::StrIdentifier: {
            const int         pos  = lex_.currPos();
            const std::string name = lex_.currS();
            lex_.advance();
            if (!check_readable_str(name, pos)) return ExprKind::String;
            emit(AsmToken::PushS, slot_str(name));
            return ExprKind::String;
        }
        // ── Numeric function call ─────────────────────────────────────────────
        case BasToken::NumFunction: {
            const std::string name = lex_.currS();
            lex_.advance();
            return compile_call(name, false);
        }
        // ── String function call ──────────────────────────────────────────────
        case BasToken::StrFunction: {
            const std::string name = lex_.currS();
            lex_.advance();
            return compile_call(name, true);
        }
        // ── Min / Max shorthand operators ─────────────────────────────────────
        case BasToken::Max: lex_.advance(); expr(); expr(); emit(AsmToken::Max); return ExprKind::Number;
        case BasToken::Min: lex_.advance(); expr(); expr(); emit(AsmToken::Min); return ExprKind::Number;

        // ── Numeric array element read:  arr[i] ───────────────────────────────
        case BasToken::Unknown: {
            const int         pos  = lex_.currPos();
            const std::string name = lex_.currS();
            lex_.advance();
            return compile_array_read(name, false, pos);
        }
        // ── String array element read:  arr$[i] ───────────────────────────────
        case BasToken::StrArray: {
            const int         pos  = lex_.currPos();
            const std::string name = lex_.currS();
            lex_.advance();
            return compile_array_read(name, true, pos);
        }

        default:
            set_error(std::format("Unexpected token '{}' in expression",
                                  std::string{ lex_.currS() }));
            return ExprKind::Number;
    }
}

// =============================================================================
// Function-call compilation  (the '(' was already consumed by the lexer)
// =============================================================================
ExprKind Parser::compile_call(const std::string& name, bool str_return) {
    // Collect arguments
    std::string param_types;
    while (!error_ &&
           lex_.currTok() != BasToken::RoundClose &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null)
    {
        auto k = expr();
        param_types += (k == ExprKind::String ? 'S' : 'N');
        if (lex_.currTok() == BasToken::Comma) lex_.advance();
    }
    if (lex_.currTok() == BasToken::RoundClose) lex_.advance();

    const std::string upper_name = utils::upper(name);
    const std::string sig        = upper_name + "@" + param_types;

    // ── Special built-ins that need executor internal state ───────────────────

    // EOF(n) — needs executor's file table
    if (upper_name == "EOF" && param_types == "N") {
        emit(AsmToken::EofFile);
        return ExprKind::Number;
    }

    // INKEY$() — non-blocking stdin char read
    if (upper_name == "INKEY$" && param_types.empty()) {
        emit(AsmToken::InkeyS);
        return ExprKind::String;
    }

    // INPUT$(n) — read exactly n chars from stdin
    if (upper_name == "INPUT$" && param_types == "N") {
        emit(AsmToken::InputN);
        return ExprKind::String;
    }

    // ── Check built-ins first ─────────────────────────────────────────────────
    if (builtins_.count(sig)) {
        const int sig_idx = intern(sig);
        emit(str_return ? AsmToken::CallFarS : AsmToken::CallFar, sig_idx);
        return str_return ? ExprKind::String : ExprKind::Number;
    }

    // ── User-defined function ─────────────────────────────────────────────────
    auto it = funcs_.find(upper_name);
    if (it != funcs_.end() && it->second.entry >= 0) {
        emit(AsmToken::CallNear, it->second.entry);
        return it->second.str_return ? ExprKind::String : ExprKind::Number;
    }

    // ── Forward reference — patch later ──────────────────────────────────────
    const int call_idx = emit(AsmToken::CallNear, 0);
    fwd_calls_.push_back({ call_idx, upper_name });
    return str_return ? ExprKind::String : ExprKind::Number;
}

// =============================================================================
// Statement compilers
// =============================================================================

// ── LET (explicit assignment) ─────────────────────────────────────────────────
void Parser::compile_let() {
    const int      src_pos = lex_.currPos();
    const BasToken id_tok  = lex_.currTok();
    const std::string name = lex_.currS();
    lex_.advance();
    // Array assignment: LET arr[i] = x  or  LET arr$[i] = x$
    if (id_tok == BasToken::Unknown) {
        compile_array_assign(name, false, src_pos); return;
    }
    if (id_tok == BasToken::StrArray) {
        compile_array_assign(name, true, src_pos); return;
    }
    compile_assign(id_tok, name);
}

// ── Assignment (LET or implicit) ─────────────────────────────────────────────
void Parser::compile_assign(BasToken id_tok, const std::string& name) {
    if (lex_.currTok() != BasToken::Equal) {
        set_error("Expected '=' in assignment"); return;
    }
    lex_.advance();

    if (id_tok == BasToken::StrIdentifier) {
        expr();
        emit(AsmToken::PopStoreS, slot_str(name));
        mark_written_str(name);
    } else {   // Identifier or PointerIdentifier treated as numeric
        expr();
        emit(AsmToken::PopStore, slot_num(name));
        mark_written_num(name);
    }
    expect_eol();
}

// ── PRINT / PRINTLN ───────────────────────────────────────────────────────────
void Parser::compile_print(bool newline) {
    // Empty PRINT → just newline
    if (lex_.currTok() == BasToken::CRLF || lex_.currTok() == BasToken::Null) {
        emit(AsmToken::CRLF);
        if (lex_.currTok() == BasToken::CRLF) lex_.advance();
        return;
    }

    bool trailing_sep = false;
    while (!error_ &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null  &&
           lex_.currTok() != BasToken::Colon)
    {
        trailing_sep = false;
        auto k = expr();
        emit(AsmToken::Print, (k == ExprKind::String) ? 1 : 0);

        if (lex_.currTok() == BasToken::Comma) {
            // Comma → print a TAB between items (classic BASIC behaviour)
            emit(AsmToken::PushCS, intern("\t"));
            emit(AsmToken::Print, 1);
            trailing_sep = true;
            lex_.advance();
        } else if (lex_.currTok() == BasToken::SemiColon) {
            // Semicolon → no separator (items printed adjacently)
            trailing_sep = true;
            lex_.advance();
        } else {
            break;
        }
    }

    if (newline || !trailing_sep)
        emit(AsmToken::CRLF);

    expect_eol();
}

// ── IF / ELSEIF / ELSE / ENDIF ───────────────────────────────────────────────
void Parser::compile_if() {
    // Condition
    expr();
    // Jump past body if false
    int jump_false = emit(AsmToken::If, 0);

    // Optional THEN on same line
    if (lex_.currTok() == BasToken::Then) {
        lex_.advance();
        // Single-line IF: rest of line is the body.
        // Supports full ELSE IF chains:
        //   IF cond THEN stmt [ELSE IF cond THEN stmt]... [ELSE stmt]
        if (lex_.currTok() != BasToken::CRLF && lex_.currTok() != BasToken::Null) {
            std::vector<int> over_jumps;  // "jump to end" from each completed arm

            compile_statement();  // THEN body

            while (lex_.currTok() == BasToken::Else) {
                // End of this arm → jump over remaining arms to the final end
                over_jumps.push_back(emit(AsmToken::Jump, 0));
                lex_.advance();  // consume ELSE

                if (lex_.currTok() == BasToken::If) {
                    // ELSE IF → new condition
                    lex_.advance();  // consume IF
                    patch(jump_false, here());
                    expr();
                    jump_false = emit(AsmToken::If, 0);
                    if (lex_.currTok() == BasToken::Then) lex_.advance();
                    compile_statement();  // body of this ELSEIF arm
                } else {
                    // Plain ELSE — last arm, no trailing jump needed from it
                    patch(jump_false, here());
                    jump_false = -1;
                    compile_statement();
                    break;
                }
            }

            if (jump_false >= 0) patch(jump_false, here());
            for (int j : over_jumps) patch(j, here());
            return;
        }
    }

    // Multi-line IF … ENDIF  (also accepts "END IF" two-token form)
    expect_eol();
    std::vector<int> end_jumps;  // all Jump-to-ENDIF patches

    auto at_endif = [&]() {
        return lex_.currTok() == BasToken::EndIf ||
               (lex_.currTok() == BasToken::End && lex_.nextTok() == BasToken::If);
    };
    auto consume_endif = [&]() {
        if (lex_.currTok() == BasToken::EndIf) { lex_.advance(); }
        else { lex_.advance(); lex_.advance(); } // END IF
    };

    // Compile body until ELSE / ELSEIF / ENDIF
    while (!error_ && lex_.currTok() != BasToken::Null) {
        skip_eols();
        if (at_endif()) { consume_endif(); break; }

        // ELSEIF <cond> [THEN]
        if (lex_.currTok() == BasToken::Identifier &&
            utils::upper(lex_.currS()) == "ELSEIF")
        {
            lex_.advance();
            end_jumps.push_back(emit(AsmToken::Jump, 0));  // jump over else-arm to end
            patch(jump_false, here());                      // false branch lands here
            expr();                                         // new condition
            jump_false = emit(AsmToken::If, 0);
            if (lex_.currTok() == BasToken::Then) lex_.advance();
            expect_eol();
            continue;
        }

        // ELSE  or  ELSE IF (two-token form of ELSEIF)
        if (lex_.currTok() == BasToken::Else) {
            lex_.advance();

            // "ELSE IF …" on the same line → treat as ELSEIF
            if (lex_.currTok() == BasToken::If) {
                lex_.advance();                                     // consume IF
                end_jumps.push_back(emit(AsmToken::Jump, 0));
                patch(jump_false, here());
                expr();                                             // new condition
                jump_false = emit(AsmToken::If, 0);
                if (lex_.currTok() == BasToken::Then) lex_.advance();
                expect_eol();
                continue;
            }

            end_jumps.push_back(emit(AsmToken::Jump, 0));
            patch(jump_false, here());
            jump_false = -1;   // already patched — don't re-patch at the bottom
            expect_eol();
            // compile else body
            while (!error_ && !at_endif() && lex_.currTok() != BasToken::Null)
                compile_statement();
            if (at_endif()) consume_endif();
            break;
        }

        compile_statement();
    }
    if (jump_false >= 0) patch(jump_false, here());
    for (int jmp : end_jumps) patch(jmp, here());
}

// ── FOR / NEXT ────────────────────────────────────────────────────────────────
void Parser::compile_for() {
    // FOR var = start TO limit [STEP step]
    if (lex_.currTok() != BasToken::Identifier) { set_error("Expected variable after FOR"); return; }
    const std::string var_name = lex_.currS();
    lex_.advance();
    const int var_slot = slot_num(var_name);
    mark_written_num(var_name);   // loop variable is always assigned by FOR

    if (lex_.currTok() != BasToken::Equal) { set_error("Expected '=' after FOR variable"); return; }
    lex_.advance();

    // start → store in var
    expr();
    emit(AsmToken::PopStore, var_slot);

    // TO limit
    if (lex_.currTok() != BasToken::To) { set_error("Expected TO in FOR"); return; }
    lex_.advance();
    expr();
    emit(AsmToken::PushAux);    // push limit to aux stack

    // STEP (optional, default 1)
    if (lex_.currTok() == BasToken::Step) {
        lex_.advance();
        expr();
    } else {
        emit(AsmToken::PushC, 0, 1.0);
    }
    emit(AsmToken::PushAux);    // push step to aux stack

    expect_eol();

    // Test
    const int test_ip = here();
    // ForCycle: var_slot stored in n (float field); i will be patched to past-loop address
    const int done_jump = emit(AsmToken::ForCycle, 0, static_cast<double>(var_slot));

    // Loop body  (terminates on NEXT, ENDFOR, or END FOR)
    auto at_next = [&]() {
        return lex_.currTok() == BasToken::Next   ||
               lex_.currTok() == BasToken::EndFor ||
               (lex_.currTok() == BasToken::End && lex_.nextTok() == BasToken::For);
    };

    loops_.push_back({ test_ip, {}, {} });
    while (!error_ && !at_next() && lex_.currTok() != BasToken::Null)
        compile_statement();

    // Patch continues to the increment
    const int incr_ip = here();
    for (int p : loops_.back().cont_patches) patch(p, incr_ip);
    // Save break-patch list BEFORE pop_back() removes it
    const std::vector<int> brk_patches = loops_.back().break_patches;
    loops_.pop_back();

    // Increment: var = var + step
    emit(AsmToken::Push,  var_slot);
    emit(AsmToken::PushAuxTOS);      // peek step (leaves it on aux)
    emit(AsmToken::Add);
    emit(AsmToken::PopStore, var_slot);
    emit(AsmToken::Jump, test_ip);

    // After loop
    patch(done_jump, here());
    emit(AsmToken::PopAux);  // remove step
    emit(AsmToken::PopAux);  // remove limit

    // Consume NEXT [var]  or  ENDFOR  or  END FOR
    if (lex_.currTok() == BasToken::Next) {
        lex_.advance();
        if (lex_.currTok() == BasToken::Identifier) lex_.advance(); // optional var name
    } else if (lex_.currTok() == BasToken::EndFor) {
        lex_.advance();
    } else if (lex_.currTok() == BasToken::End) {
        lex_.advance(); lex_.advance(); // END FOR
    }
    // Patch breaks to past-loop address
    for (int p : brk_patches) patch(p, here());
    expect_eol();
}

// ── WHILE / ENDWHILE  (also: END WHILE) ──────────────────────────────────────
void Parser::compile_while() {
    const int test_ip = here();
    expr();
    const int done_jump = emit(AsmToken::If, 0);   // jump past body if false
    expect_eol();

    auto at_endwhile = [&]() {
        return lex_.currTok() == BasToken::EndWhile ||
               (lex_.currTok() == BasToken::End && lex_.nextTok() == BasToken::While);
    };

    loops_.push_back({ test_ip, {}, {} });
    while (!error_ && !at_endwhile() && lex_.currTok() != BasToken::Null)
        compile_statement();

    for (int p : loops_.back().cont_patches) patch(p, test_ip);
    const std::vector<int> brk = loops_.back().break_patches;
    loops_.pop_back();

    emit(AsmToken::Jump, test_ip);
    patch(done_jump, here());
    for (int p : brk) patch(p, here());

    // Consume ENDWHILE  or  END WHILE
    if (lex_.currTok() == BasToken::EndWhile) lex_.advance();
    else if (lex_.currTok() == BasToken::End) { lex_.advance(); lex_.advance(); }
    expect_eol();
}

// ── DO / LOOP ─────────────────────────────────────────────────────────────────
void Parser::compile_do() {
    // Forms:
    //   DO WHILE cond … LOOP
    //   DO UNTIL cond … LOOP
    //   DO … LOOP WHILE cond
    //   DO … LOOP UNTIL cond

    const bool pre_while = (lex_.currTok() == BasToken::While);
    const bool pre_until = (lex_.currTok() == BasToken::Until);
    int pre_jump = -1;
    const int start_ip = here();

    if (pre_while || pre_until) {
        lex_.advance();
        expr();
        if (pre_while)
            pre_jump = emit(AsmToken::If, 0);    // exit if false
        else {
            emit(AsmToken::Not);
            pre_jump = emit(AsmToken::If, 0);    // exit if true (until = exit when true)
        }
    }

    expect_eol();
    loops_.push_back({ start_ip, {}, {} });

    while (!error_ &&
           lex_.currTok() != BasToken::Loop &&
           lex_.currTok() != BasToken::Null)
        compile_statement();

    skip_eols();
    for (int p : loops_.back().cont_patches) patch(p, here());
    const std::vector<int> brk = loops_.back().break_patches;
    loops_.pop_back();

    if (lex_.currTok() == BasToken::Loop) {
        lex_.advance();
        if (lex_.currTok() == BasToken::While) {
            lex_.advance();
            expr();
            emit(AsmToken::Not);
            emit(AsmToken::If, start_ip);   // while: jump back if true
        } else if (lex_.currTok() == BasToken::Until) {
            lex_.advance();
            expr();
            emit(AsmToken::If, start_ip);   // until: jump back if false
        } else {
            emit(AsmToken::Jump, start_ip);
        }
    }

    const int after_ip = here();
    if (pre_jump >= 0) patch(pre_jump, after_ip);
    for (int p : brk) patch(p, after_ip);
    expect_eol();
}

// ── REPEAT / UNTIL ────────────────────────────────────────────────────────────
void Parser::compile_repeat() {
    expect_eol();
    const int start_ip = here();
    loops_.push_back({ -1, {}, {} });

    while (!error_ &&
           lex_.currTok() != BasToken::Until &&
           lex_.currTok() != BasToken::Null)
        compile_statement();

    skip_eols();
    for (int p : loops_.back().cont_patches) patch(p, here());
    const std::vector<int> brk = loops_.back().break_patches;
    loops_.pop_back();

    if (lex_.currTok() == BasToken::Until) {
        lex_.advance();
        expr();
        emit(AsmToken::If, start_ip);   // jump back while condition is false
    }
    for (int p : brk) patch(p, here());
    expect_eol();
}

// ── FUNCTION / ENDFUNCTION ────────────────────────────────────────────────────
void Parser::compile_function() {
    // FUNCTION name( params )  [LOCAL var, var$ …]
    // Accepts:  FUNCTION name(…)          numeric-returning
    //           FUNCTION name$(…)         string-returning
    const bool str_ret = (lex_.currTok() == BasToken::StrFunction);
    const bool is_func_tok = (lex_.currTok() == BasToken::NumFunction ||
                               lex_.currTok() == BasToken::StrFunction);
    if (!is_func_tok) { set_error("Expected function name after FUNCTION"); return; }

    const std::string raw_name  = lex_.currS();
    const std::string func_name = utils::upper(raw_name);
    lex_.advance();

    // Collect parameter names (lexer already consumed the opening '(')
    std::vector<std::pair<std::string,bool>> params; // (name, is_string)
    while (!error_ &&
           lex_.currTok() != BasToken::RoundClose &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null)
    {
        if (lex_.currTok() == BasToken::Identifier) {
            params.push_back({ lex_.currS(), false });
            lex_.advance();
        } else if (lex_.currTok() == BasToken::StrIdentifier) {
            params.push_back({ lex_.currS(), true });
            lex_.advance();
        } else if (lex_.currTok() == BasToken::Comma) {
            lex_.advance();
        } else {
            lex_.advance(); // skip unexpected token
        }
    }
    if (lex_.currTok() == BasToken::RoundClose) lex_.advance();

    // ── Optional LOCAL declaration on the same header line ────────────────────
    // e.g.  FUNCTION foo(x) LOCAL i, tmp$
    // We collect the names here and register them as locals after in_func_=true.
    std::vector<std::string> extra_locals_num, extra_locals_str;
    if (lex_.currTok() == BasToken::Local) {
        lex_.advance(); // consume LOCAL
        while (lex_.currTok() != BasToken::CRLF && lex_.currTok() != BasToken::Null) {
            if (lex_.currTok() == BasToken::StrIdentifier) {
                extra_locals_str.push_back(lex_.currS()); lex_.advance();
            } else if (lex_.currTok() == BasToken::Identifier) {
                extra_locals_num.push_back(lex_.currS()); lex_.advance();
            } else {
                lex_.advance(); // comma or unexpected
            }
        }
    }
    expect_eol();

    // Emit Jump to skip over the function body during top-level execution
    const int skip_jump = emit(AsmToken::Jump, 0);

    // Record function entry
    const int entry_ip = here();
    FuncInfo& fi = funcs_[func_name];
    fi.entry      = entry_ip;
    fi.params     = static_cast<int>(params.size());
    fi.str_return = str_ret;

    // Patch any previously emitted forward calls to this function
    std::erase_if(fwd_calls_, [&](const std::pair<int,std::string>& p) {
        if (p.second == func_name) { patch(p.first, entry_ip); return true; }
        return false;
    });

    // InitFunc: string-pool index of param type signature (e.g. "SN" for str+num)
    // Exec uses this to correctly distribute args into lnum vs lstr arrays.
    std::string type_sig;
    for (auto& [pname, pstr] : params) type_sig += (pstr ? 'S' : 'N');
    emit(AsmToken::InitFunc, intern(type_sig));

    // Enter local scope
    in_func_ = true;
    lnum_.clear(); lstr_.clear();
    lnum_def_.clear(); lstr_def_.clear();   // reset local definition tracking
    lnnext_ = lsnext_ = 0;

    // Bind parameters as locals (in declaration order) — params are always written.
    // These must occupy the lowest-numbered local slots so that InitFunc (exec)
    // can distribute the runtime argument values into lnum[0..] / lstr[0..] in
    // declaration order, matching the type signature that was emitted.
    for (auto& [pname, pstr] : params) {
        if (pstr) { slot_str(pname); mark_written_str(pname); }
        else      { slot_num(pname); mark_written_num(pname); }
    }
    // Register locals declared on the header line (LOCAL keyword)
    // LOCAL vars are runtime-initialized to 0/"", so they count as "written".
    for (const auto& n : extra_locals_num) { slot_num(n); mark_written_num(n); }
    for (const auto& s : extra_locals_str) { slot_str(s); mark_written_str(s); }

    // ── Compile body ──────────────────────────────────────────────────────────
    // Terminator: ENDFUNCTION  or  END FUNCTION  (two-token form)
    while (!error_ && lex_.currTok() != BasToken::Null) {
        skip_eols();
        if (lex_.currTok() == BasToken::EndFunction) break;
        // "END FUNCTION" written as two tokens
        if (lex_.currTok() == BasToken::End) {
            lex_.advance();
            if (lex_.currTok() == BasToken::Function) break; // END FUNCTION
            // Genuine END statement inside the function
            emit(AsmToken::End);
            expect_eol();
            continue;
        }
        compile_statement();
    }

    // Implicit return: if the body assigned to the function return variable
    // (same name as the function), push that local slot; otherwise push the
    // default 0 / "".  Explicit RETURN statements already emit Push+RetFunction
    // and exit early, so this path is only reached on fall-through.
    // lnum_/lstr_ are now keyed by uppercase (func_name, not raw_name).
    if (str_ret) {
        auto lit = lstr_.find(func_name);
        if (lit != lstr_.end())
            emit(AsmToken::PushS, -(lit->second + 1));
        else
            emit(AsmToken::PushCS, intern(""));
    } else {
        auto lit = lnum_.find(func_name);
        if (lit != lnum_.end())
            emit(AsmToken::Push, -(lit->second + 1));
        else
            emit(AsmToken::PushC, 0, 0.0);
    }
    emit(AsmToken::RetFunction);

    // Consume the closing keyword(s)
    if (lex_.currTok() == BasToken::EndFunction) lex_.advance();      // ENDFUNCTION
    else if (lex_.currTok() == BasToken::Function) lex_.advance();    // END FUNCTION (END was consumed above)

    in_func_ = false;
    lnum_def_.clear(); lstr_def_.clear();   // clean up local tracking

    // Patch the skip jump so execution resumes after the function body
    patch(skip_jump, here());
    expect_eol();
}

// ── RETURN ────────────────────────────────────────────────────────────────────
void Parser::compile_return() {
    if (in_func_) {
        // RETURN expr
        if (lex_.currTok() != BasToken::CRLF && lex_.currTok() != BasToken::Null)
            expr();
        else
            emit(AsmToken::PushC, 0, 0.0);
        emit(AsmToken::RetFunction);
    } else {
        emit(AsmToken::Return);
    }
    expect_eol();
}

// ── GOSUB ─────────────────────────────────────────────────────────────────────
void Parser::compile_gosub() {
    std::string lbl = lex_.currS();
    if (lex_.currTok() == BasToken::Label && !lbl.empty() && lbl.back() == ':')
        lbl.pop_back();
    lex_.advance();

    auto it = labels_.find(lbl);
    if (it != labels_.end()) {
        emit(AsmToken::CallNear, it->second);   // reuse CallNear for GOSUB
    } else {
        const int idx = emit(AsmToken::CallNear, 0);
        fwd_gosubs_.push_back({ idx, lbl });
    }
    expect_eol();
}

// ── GOTO ──────────────────────────────────────────────────────────────────────
void Parser::compile_goto() {
    std::string lbl = lex_.currS();
    if (lex_.currTok() == BasToken::Label && !lbl.empty() && lbl.back() == ':')
        lbl.pop_back();
    lex_.advance();

    auto it = labels_.find(lbl);
    if (it != labels_.end()) {
        emit(AsmToken::Jump, it->second);
    } else {
        const int idx = emit(AsmToken::Jump, 0);
        fwd_gotos_.push_back({ idx, lbl });
    }
    expect_eol();
}

// ── Label definition ──────────────────────────────────────────────────────────
void Parser::compile_label(const std::string& name) {
    const int ip = here();
    labels_[name] = ip;
    // Patch waiting GOTOs/GOSUBs
    std::erase_if(fwd_gotos_,  [&](const std::pair<int,std::string>& p){
        if (p.second == name) { patch(p.first, ip); return true; } return false; });
    std::erase_if(fwd_gosubs_, [&](const std::pair<int,std::string>& p){
        if (p.second == name) { patch(p.first, ip); return true; } return false; });
}

// ── INPUT ─────────────────────────────────────────────────────────────────────
void Parser::compile_input() {
    // INPUT ["prompt" ;|,] var [, var2, ...]
    // The optional string prompt is printed once before reading the first value.
    // Each variable gets its own read (one line per variable).
    if (lex_.currTok() == BasToken::String) {
        const int pidx = intern(lex_.currS());
        lex_.advance();
        emit(AsmToken::PushCS, pidx);
        emit(AsmToken::Print, 1);   // 1 = force string print
        if (lex_.currTok() == BasToken::SemiColon || lex_.currTok() == BasToken::Comma)
            lex_.advance();
    } else {
        // Default prompt "? "
        emit(AsmToken::PushCS, intern("? "));
        emit(AsmToken::Print, 1);
    }

    // Read one or more comma-separated variables.
    while (!error_ &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null)
    {
        if (lex_.currTok() == BasToken::StrIdentifier) {
            const std::string vname = lex_.currS(); lex_.advance();
            mark_written_str(vname);
            emit(AsmToken::InputS, slot_str(vname));
        } else if (lex_.currTok() == BasToken::Identifier) {
            const std::string vname = lex_.currS(); lex_.advance();
            mark_written_num(vname);
            emit(AsmToken::Input, slot_num(vname));
        } else {
            break;   // unexpected token — stop quietly
        }

        if (lex_.currTok() == BasToken::Comma) lex_.advance();
        else break;
    }
    expect_eol();
}

// ── DATA ──────────────────────────────────────────────────────────────────────
void Parser::compile_data() {
    while (lex_.currTok() != BasToken::CRLF && lex_.currTok() != BasToken::Null) {
        DataItem di;
        di.dataPos = here();
        if (lex_.currTok() == BasToken::String) {
            di.dataType = '$';
            const int idx = intern(lex_.currS());
            lex_.advance();
            emit(AsmToken::DataS, idx);
        } else {
            di.dataType = 'n';
            const double v = lex_.currN();
            lex_.advance();
            emit(AsmToken::Data, 0, v);
        }
        prog_.data.push_back(di);
        if (lex_.currTok() == BasToken::Comma) lex_.advance();
    }
    expect_eol();
}

// ── READ ──────────────────────────────────────────────────────────────────────
void Parser::compile_read() {
    while (lex_.currTok() != BasToken::CRLF && lex_.currTok() != BasToken::Null) {
        if (lex_.currTok() == BasToken::StrIdentifier) {
            const std::string vname = lex_.currS(); lex_.advance();
            mark_written_str(vname);   // READ writes to this variable
            emit(AsmToken::ReadS, slot_str(vname));
        } else if (lex_.currTok() == BasToken::Identifier) {
            const std::string vname = lex_.currS(); lex_.advance();
            mark_written_num(vname);   // READ writes to this variable
            emit(AsmToken::Read, slot_num(vname));
        } else {
            lex_.advance();
        }
        if (lex_.currTok() == BasToken::Comma) lex_.advance();
    }
    expect_eol();
}

// ── LOCAL ─────────────────────────────────────────────────────────────────────
void Parser::compile_local() {
    while (lex_.currTok() != BasToken::CRLF && lex_.currTok() != BasToken::Null) {
        if (lex_.currTok() == BasToken::StrIdentifier) {
            const std::string vname = lex_.currS();
            slot_str(vname);
            mark_written_str(vname);   // LOCAL vars initialized to "" by runtime
        } else if (lex_.currTok() == BasToken::Identifier) {
            const std::string vname = lex_.currS();
            slot_num(vname);
            mark_written_num(vname);   // LOCAL vars initialized to 0 by runtime
        }
        lex_.advance();
        if (lex_.currTok() == BasToken::Comma) lex_.advance();
    }
    expect_eol();
}

// ── BREAK / CONTINUE ─────────────────────────────────────────────────────────
void Parser::compile_break() {
    if (loops_.empty()) { set_error("BREAK outside loop"); return; }
    loops_.back().break_patches.push_back(emit(AsmToken::Jump, 0));
    expect_eol();
}

void Parser::compile_continue() {
    if (loops_.empty()) { set_error("CONTINUE outside loop"); return; }
    const int cont_ip = loops_.back().continue_ip;
    if (cont_ip >= 0)
        emit(AsmToken::Jump, cont_ip);
    else
        loops_.back().cont_patches.push_back(emit(AsmToken::Jump, 0));
    expect_eol();
}

// ── SELECT CASE ───────────────────────────────────────────────────────────────
//
// SELECT CASE expr
//   CASE val [, val2 ...] [TO val3] [, IS relop val4] ...
//     statements
//   CASE val5
//     statements
//   CASE ELSE
//     statements
// END SELECT   (or ENDSELECT)
//
// Strategy: evaluate selector once into a hidden temp variable, then for each
// CASE clause emit comparison bytecode + conditional jump.  Each case body
// ends with an unconditional jump to END SELECT.  CASE ELSE has no condition.
// Nesting is supported via select_depth_ counter naming separate temp vars.
void Parser::compile_select() {
    // Consume "CASE" that follows "SELECT"
    if (lex_.currTok() == BasToken::Case) lex_.advance();

    // Unique hidden temp-variable names for the selector value at this depth.
    const int depth = select_depth_++;
    const std::string tmp_n = "__sel_n_" + std::to_string(depth);
    const std::string tmp_s = "__sel_s_" + std::to_string(depth);

    // Evaluate the selector expression and store it.
    const ExprKind sel_kind = expr();
    const bool is_str = (sel_kind == ExprKind::String);
    const int  sel_slot = is_str ? slot_str(tmp_s) : slot_num(tmp_n);
    emit(is_str ? AsmToken::PopStoreS : AsmToken::PopStore, sel_slot);
    if (is_str) mark_written_str(tmp_s); else mark_written_num(tmp_n);
    expect_eol();

    std::vector<int> end_jumps;   // Jump instructions to patch → END SELECT
    int next_cond_jump = -1;      // If instruction to patch → next CASE condition

    skip_eols();

    while (!error_ && lex_.currTok() != BasToken::Null) {
        skip_eols();

        // Detect block terminator (one-token or two-token form) — consumes the token(s).
        if (lex_.currTok() == BasToken::EndSelect) { lex_.advance(); goto select_done; }
        if (lex_.currTok() == BasToken::End && lex_.nextTok() == BasToken::Select) {
            lex_.advance(); lex_.advance(); goto select_done;
        }

        if (lex_.currTok() != BasToken::Case) {
            lex_.advance();   // skip stray token
            continue;
        }
        lex_.advance();   // consume CASE

        // Patch previous CASE's "not matched" jump to here (start of this CASE).
        if (next_cond_jump >= 0) { patch(next_cond_jump, here()); next_cond_jump = -1; }

        {
            bool is_else = false;
            if (lex_.currTok() == BasToken::Else) {
                // CASE ELSE — unconditional fallthrough, must be last clause.
                lex_.advance();
                is_else = true;
                expect_eol();
            } else {
                // Compile the condition list; leaves a bool (1.0/0.0) on the stack.
                compile_case_condition(sel_slot, is_str);
                // If condition is false, jump past this body to the next CASE.
                next_cond_jump = emit(AsmToken::If, 0);
            }

            // Compile the case body until the next CASE / END SELECT / EOF.
            while (!error_ && lex_.currTok() != BasToken::Null) {
                skip_eols();
                if (lex_.currTok() == BasToken::Case)      break;
                if (lex_.currTok() == BasToken::EndSelect)  break;
                if (lex_.currTok() == BasToken::End && lex_.nextTok() == BasToken::Select) break;
                compile_statement();
            }

            // End of body: jump over remaining cases to END SELECT.
            end_jumps.push_back(emit(AsmToken::Jump, 0));

            if (is_else) {
                // CASE ELSE is the last clause — consume END SELECT if present.
                skip_eols();
                if (lex_.currTok() == BasToken::EndSelect) {
                    lex_.advance();
                } else if (lex_.currTok() == BasToken::End &&
                           lex_.nextTok() == BasToken::Select) {
                    lex_.advance(); lex_.advance();
                }
                goto select_done;
            }
        }
    }

    select_done:

    // Patch any remaining "not matched" jump (last regular CASE, no ELSE).
    if (next_cond_jump >= 0) patch(next_cond_jump, here());

    // Patch all end-of-body jumps to here (just past END SELECT).
    for (int j : end_jumps) patch(j, here());

    --select_depth_;
}

// Compile a CASE condition list leaving a final bool (1.0/0.0) on the stack.
// Items are comma-separated; each can be:
//   val          — equality:  sel == val
//   val TO val2  — range:     sel >= val AND sel <= val2
//   IS relop val — relation:  sel relop val
// Multiple items are combined with logical OR.
void Parser::compile_case_condition(int sel_slot, bool is_str) {
    // Lambda: push the selector value onto the stack.
    auto push_sel = [&]() {
        emit(is_str ? AsmToken::PushS : AsmToken::Push, sel_slot);
    };

    bool first = true;
    while (!error_ &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null)
    {
        // ── IS relop val ─────────────────────────────────────────────────────
        if (lex_.currTok() == BasToken::Identifier &&
            utils::upper(lex_.currS()) == "IS")
        {
            lex_.advance();   // consume IS
            const BasToken rel = lex_.currTok();
            lex_.advance();   // consume relational operator

            push_sel();
            expr();   // right-hand value

            AsmToken cmp;
            switch (rel) {
                case BasToken::Equal:        cmp = is_str ? AsmToken::EqS : AsmToken::Eq;  break;
                case BasToken::NotEqual:     cmp = is_str ? AsmToken::NeS : AsmToken::Ne;  break;
                case BasToken::Lower:        cmp = is_str ? AsmToken::LtS : AsmToken::Lt;  break;
                case BasToken::LowerEqual:   cmp = is_str ? AsmToken::LeS : AsmToken::Le;  break;
                case BasToken::Greater:      cmp = is_str ? AsmToken::GtS : AsmToken::Gt;  break;
                case BasToken::GreaterEqual: cmp = is_str ? AsmToken::GeS : AsmToken::Ge;  break;
                default:
                    set_error("Expected relational operator after IS in CASE");
                    return;
            }
            emit(cmp);
        }
        // ── val [TO val2] ─────────────────────────────────────────────────────
        else {
            push_sel();
            expr();   // lower bound (or single value)

            if (lex_.currTok() == BasToken::To) {
                // Range: sel >= lower AND sel <= upper
                lex_.advance();   // consume TO
                emit(is_str ? AsmToken::GeS : AsmToken::Ge);   // sel >= lower

                push_sel();
                expr();   // upper bound
                emit(is_str ? AsmToken::LeS : AsmToken::Le);   // sel <= upper
                emit(AsmToken::And);
            } else {
                // Single-value equality: sel == val
                emit(is_str ? AsmToken::EqS : AsmToken::Eq);
            }
        }

        // Combine multiple items in the list with logical OR.
        if (!first) emit(AsmToken::Or);
        first = false;

        if (lex_.currTok() == BasToken::Comma) lex_.advance();
        else break;
    }
    expect_eol();
}

// ── ON expr GOTO / GOSUB ──────────────────────────────────────────────────────
//
// ON expr GOTO  label1 [, label2 ...]
// ON expr GOSUB label1 [, label2 ...]
//
// Evaluates expr (1-based selector).  If 1 ≤ expr ≤ N, jumps to (or calls via
// GOSUB) the N-th label.  If expr is out of range, execution falls through.
//
// Bytecode emitted:
//   [expr]
//   OnGoto N    (or OnGosub N)
//   Jump lbl1   ← target for selector == 1
//   Jump lbl2   ← target for selector == 2
//   ...
//   Jump lblN
//   [continues here for out-of-range selector]
void Parser::compile_on() {
    // ── ON ERROR GOTO label / ON ERROR GOTO 0 ───────────────────────────────
    if (lex_.currTok() == BasToken::Error) {
        lex_.advance();  // consume ERROR
        if (lex_.currTok() != BasToken::Goto) {
            set_error("Expected GOTO after ON ERROR");
            return;
        }
        lex_.advance();  // consume GOTO
        // ON ERROR GOTO 0 → disable handler
        if (lex_.currTok() == BasToken::Integer && lex_.currN() == 0.0) {
            lex_.advance();
            emit(AsmToken::OnError, 0);
            expect_eol();
            return;
        }
        // ON ERROR GOTO label
        std::string lbl = lex_.currS();
        if (lex_.currTok() == BasToken::Label || lex_.currTok() == BasToken::Identifier) {
            if (!lbl.empty() && lbl.back() == ':') lbl.pop_back();
        } else {
            set_error("ON ERROR GOTO: expected label");
            return;
        }
        lex_.advance();
        const int patch_ip = emit(AsmToken::OnError, 0);
        auto it = labels_.find(lbl);
        if (it != labels_.end()) {
            patch(patch_ip, it->second);
        } else {
            // Forward reference — reuse fwd_gotos_ queue; patched after compile
            fwd_gotos_.push_back({ patch_ip, lbl });
            // But OnError uses .i, not as a jump target... we need special patch.
            // Since patch() sets .i, and OnError also uses .i, this works correctly.
        }
        expect_eol();
        return;
    }

    // ── ON expr GOTO / ON expr GOSUB ─────────────────────────────────────────
    expr();   // selector value (1-based)

    const bool is_gosub = (lex_.currTok() == BasToken::Gosub);
    if      (lex_.currTok() == BasToken::Goto)  lex_.advance();
    else if (lex_.currTok() == BasToken::Gosub) lex_.advance();
    else {
        set_error("Expected GOTO or GOSUB after ON expression");
        return;
    }

    // Emit the dispatch instruction; patch .i with the count after all labels.
    const int on_ip = emit(is_gosub ? AsmToken::OnGosub : AsmToken::OnGoto, 0);

    int n = 0;
    while (!error_ &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null)
    {
        // Accept both "label:" (Label token) and bare "label" (Identifier token)
        std::string lbl = lex_.currS();
        if ((lex_.currTok() == BasToken::Label || lex_.currTok() == BasToken::Identifier) &&
            !lbl.empty() && lbl.back() == ':')
            lbl.pop_back();
        lex_.advance();

        const int jump_ip = emit(AsmToken::Jump, 0);
        auto it = labels_.find(lbl);
        if (it != labels_.end()) {
            patch(jump_ip, it->second);
        } else {
            // Forward reference — same backpatch queue as regular GOTO.
            fwd_gotos_.push_back({ jump_ip, lbl });
        }
        ++n;

        if (lex_.currTok() == BasToken::Comma) lex_.advance();
        else break;
    }

    // Patch the dispatch instruction with the target count.
    patch(on_ip, n);
    expect_eol();
}

// =============================================================================
// Phase 3 statement compilers
// =============================================================================

// ── SHELL "command" ──────────────────────────────────────────────────────────
void Parser::compile_shell() {
    const auto k = expr();
    if (k != ExprKind::String) {
        set_error("SHELL: expected string expression");
        return;
    }
    emit(AsmToken::ShellCmd);
    expect_eol();
}

// ── RESUME [NEXT | label:] ────────────────────────────────────────────────────
void Parser::compile_resume() {
    if (lex_.currTok() == BasToken::CRLF || lex_.currTok() == BasToken::Null) {
        // RESUME — retry the error statement
        emit(AsmToken::ResumeStmt, 0);
    } else if (lex_.currTok() == BasToken::Next) {
        // RESUME NEXT — continue after the error statement
        lex_.advance();
        emit(AsmToken::ResumeStmt, 1);
    } else {
        // RESUME label: — jump to specific label
        std::string lbl = lex_.currS();
        if (lex_.currTok() == BasToken::Label || lex_.currTok() == BasToken::Identifier) {
            if (!lbl.empty() && lbl.back() == ':') lbl.pop_back();
            lex_.advance();
        } else {
            set_error("RESUME: expected NEXT or label");
            return;
        }
        const int patch_ip = emit(AsmToken::ResumeStmt, 2, 0.0);
        auto it = labels_.find(lbl);
        if (it != labels_.end()) {
            // Store target in instr.n (reuse; executor reads instr.n for mode 2)
            prog_.code[static_cast<std::size_t>(patch_ip)].n =
                static_cast<double>(it->second);
        } else {
            // Forward reference — store patch_ip + special marker in fwd_gotos_
            // We'll fix up instr.n after compile; use a custom entry
            // Reuse fwd_gotos_ but patch .n instead of .i — add dedicated list
            fwd_gotos_.push_back({ patch_ip, "__resume__" + lbl });
        }
    }
    expect_eol();
}

// ── WRITE #n, expr [, expr ...] ───────────────────────────────────────────────
void Parser::compile_write_file() {
    // WRITE #n — file number is mandatory
    if (lex_.currTok() != BasToken::Hash) {
        set_error("WRITE: expected #n file number");
        return;
    }
    const int fn = parse_file_num();
    if (error_) return;

    // Optional comma after #n
    if (lex_.currTok() == BasToken::Comma) lex_.advance();

    if (lex_.currTok() == BasToken::CRLF || lex_.currTok() == BasToken::Null) {
        emit(AsmToken::WriteFileCRLF, 0, static_cast<double>(fn));
        if (lex_.currTok() == BasToken::CRLF) lex_.advance();
        return;
    }

    bool trailing_sep = false;
    while (!error_ &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null  &&
           lex_.currTok() != BasToken::Colon)
    {
        trailing_sep = false;
        const auto k = expr();
        emit(AsmToken::WriteFile, (k == ExprKind::String) ? 1 : 0,
             static_cast<double>(fn));

        if (lex_.currTok() == BasToken::Comma) {
            emit(AsmToken::WriteFileSep, 0, static_cast<double>(fn));
            trailing_sep = true;
            lex_.advance();
        } else if (lex_.currTok() == BasToken::SemiColon) {
            trailing_sep = true;
            lex_.advance();
        } else {
            break;
        }
    }

    if (!trailing_sep)
        emit(AsmToken::WriteFileCRLF, 0, static_cast<double>(fn));

    expect_eol();
}

// ── PRINT USING fmtexpr ; val [; val ...] ─────────────────────────────────────
// Called after 'PRINT USING' has been consumed.
void Parser::compile_print_using(bool newline) {
    // Compile the format expression (string)
    const auto fk = expr();
    if (fk != ExprKind::String) {
        set_error("PRINT USING: format must be a string expression");
        return;
    }
    // Consume separator between format string and first value
    if (lex_.currTok() == BasToken::SemiColon || lex_.currTok() == BasToken::Comma)
        lex_.advance();

    // Format string is now on the stack.
    // For each value, compile it and emit PrintUsing.
    bool trailing_sep = false;
    while (!error_ &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null  &&
           lex_.currTok() != BasToken::Colon)
    {
        trailing_sep = false;
        const auto k = expr();
        emit(AsmToken::PrintUsing, (k == ExprKind::String) ? 1 : 0);

        if (lex_.currTok() == BasToken::SemiColon) {
            trailing_sep = true;
            lex_.advance();
        } else if (lex_.currTok() == BasToken::Comma) {
            trailing_sep = true;
            lex_.advance();
        } else {
            break;
        }
    }

    // PrintUsingEnd: i=1 → newline, i=0 → no newline
    emit(AsmToken::PrintUsingEnd, (newline || !trailing_sep) ? 1 : 0);
    expect_eol();
}

// =============================================================================
// File I/O statement compilers
// =============================================================================

// ── parse_file_num ─────────────────────────────────────────────────────────
// Parses and consumes a #n token. Returns the file number (>= 1),
// or -1 and sets error on bad syntax.
int Parser::parse_file_num() {
    if (lex_.currTok() != BasToken::Hash) {
        set_error("Expected #n file number");
        return -1;
    }
    const int fn = static_cast<int>(lex_.currN());
    lex_.advance();
    if (fn < 1) {
        set_error("File number must be >= 1");
        return -1;
    }
    return fn;
}

// ── OPEN "filename" FOR INPUT|OUTPUT|APPEND AS #n ─────────────────────────
void Parser::compile_open() {
    if (lex_.currTok() != BasToken::String) {
        set_error("OPEN: expected filename string");
        return;
    }
    const int fname_idx = intern(lex_.currS());
    lex_.advance();
    emit(AsmToken::PushCS, fname_idx);

    if (lex_.currTok() != BasToken::For) {
        set_error("OPEN: expected FOR");
        return;
    }
    lex_.advance();

    int mode = -1;
    if (lex_.currTok() == BasToken::Input) {
        mode = 0; lex_.advance();
    } else if (lex_.currTok() == BasToken::Output) {
        mode = 1; lex_.advance();
    } else if (lex_.currTok() == BasToken::Append) {
        mode = 2; lex_.advance();
    } else {
        set_error("OPEN: expected INPUT, OUTPUT, or APPEND after FOR");
        return;
    }

    if (lex_.currTok() != BasToken::As) {
        set_error("OPEN: expected AS");
        return;
    }
    lex_.advance();

    const int fn = parse_file_num();
    if (error_) return;

    emit(AsmToken::OpenFile, mode, static_cast<double>(fn));
    expect_eol();
}

// ── CLOSE [#n [, #m ...]]  ─────────────────────────────────────────────────
void Parser::compile_close() {
    if (lex_.currTok() == BasToken::CRLF ||
        lex_.currTok() == BasToken::Null ||
        lex_.currTok() == BasToken::Colon)
    {
        // Bare CLOSE → close all
        emit(AsmToken::CloseFile, 1, 0.0);
    } else {
        // CLOSE #n [, #m ...]
        while (!error_) {
            const int fn = parse_file_num();
            if (error_) return;
            emit(AsmToken::CloseFile, 0, static_cast<double>(fn));
            if (lex_.currTok() == BasToken::Comma) lex_.advance();
            else break;
        }
    }
    expect_eol();
}

// ── PRINT #n [, expr [; expr ...]] ────────────────────────────────────────
void Parser::compile_print_file(int fn) {
    // Skip optional comma separator between #n and first item
    if (lex_.currTok() == BasToken::Comma) lex_.advance();

    if (lex_.currTok() == BasToken::CRLF || lex_.currTok() == BasToken::Null) {
        emit(AsmToken::PrintFileCRLF, 0, static_cast<double>(fn));
        if (lex_.currTok() == BasToken::CRLF) lex_.advance();
        return;
    }

    bool trailing_sep = false;
    while (!error_ &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null  &&
           lex_.currTok() != BasToken::Colon)
    {
        trailing_sep = false;
        const auto k = expr();
        emit(AsmToken::PrintFile, (k == ExprKind::String) ? 1 : 0,
             static_cast<double>(fn));

        if (lex_.currTok() == BasToken::Comma) {
            emit(AsmToken::PrintFileSep, 0, static_cast<double>(fn));
            trailing_sep = true;
            lex_.advance();
        } else if (lex_.currTok() == BasToken::SemiColon) {
            trailing_sep = true;
            lex_.advance();
        } else {
            break;
        }
    }

    if (!trailing_sep)
        emit(AsmToken::PrintFileCRLF, 0, static_cast<double>(fn));

    expect_eol();
}

// ── INPUT #n, var [, var ...] ─────────────────────────────────────────────
void Parser::compile_input_file(int fn) {
    // Skip optional comma separator between #n and first variable
    if (lex_.currTok() == BasToken::Comma) lex_.advance();

    while (!error_ &&
           lex_.currTok() != BasToken::CRLF &&
           lex_.currTok() != BasToken::Null)
    {
        if (lex_.currTok() == BasToken::StrIdentifier) {
            const std::string vname = lex_.currS();
            lex_.advance();
            mark_written_str(vname);
            emit(AsmToken::InputFileS, slot_str(vname), static_cast<double>(fn));
        } else if (lex_.currTok() == BasToken::Identifier) {
            const std::string vname = lex_.currS();
            lex_.advance();
            mark_written_num(vname);
            emit(AsmToken::InputFile, slot_num(vname), static_cast<double>(fn));
        } else {
            break;
        }
        if (lex_.currTok() == BasToken::Comma) lex_.advance();
        else break;
    }
    expect_eol();
}

// ── LINE INPUT [#n,] var$ ─────────────────────────────────────────────────
// 'LINE' token was already consumed by compile_statement.
void Parser::compile_line_input() {
    if (lex_.currTok() != BasToken::Input) {
        set_error("Expected INPUT after LINE");
        return;
    }
    lex_.advance();

    if (lex_.currTok() == BasToken::Hash) {
        // LINE INPUT #n, var$
        const int fn = parse_file_num();
        if (error_) return;
        if (lex_.currTok() == BasToken::Comma) lex_.advance();
        if (lex_.currTok() != BasToken::StrIdentifier) {
            set_error("LINE INPUT #n: expected string variable");
            return;
        }
        const std::string vname = lex_.currS();
        lex_.advance();
        mark_written_str(vname);
        emit(AsmToken::LineInputFile, slot_str(vname), static_cast<double>(fn));
    } else {
        // LINE INPUT var$ — read whole line from stdin
        if (lex_.currTok() == BasToken::Comma) lex_.advance();
        if (lex_.currTok() != BasToken::StrIdentifier) {
            set_error("LINE INPUT: expected string variable");
            return;
        }
        const std::string vname = lex_.currS();
        lex_.advance();
        mark_written_str(vname);
        emit(AsmToken::InputS, slot_str(vname));
    }
    expect_eol();
}

// =============================================================================
// Phase 6 — LOCATE, COLOR
// =============================================================================

void Parser::compile_locate() {
    expr();   // row
    if (lex_.currTok() != BasToken::Comma) { set_error("LOCATE: expected ','"); return; }
    lex_.advance();
    expr();   // col
    emit(AsmToken::LocateXY);
}

void Parser::compile_color() {
    expr();   // fg (0-15)
    if (lex_.currTok() == BasToken::Comma) {
        lex_.advance();
        expr();   // bg (0-7)
        emit(AsmToken::ColorSet, 1);
    } else {
        emit(AsmToken::PushC, 0, -1.0);   // sentinel: no bg
        emit(AsmToken::ColorSet, 0);
    }
}

// =============================================================================
// Phase 8 — ASSERT
// =============================================================================

void Parser::compile_assert() {
    expr();  // condition → number on stack
    if (lex_.currTok() == BasToken::Comma) {
        lex_.advance();
        auto k = expr();  // message → string on stack
        if (k != ExprKind::String) {
            set_error("ASSERT: second argument must be a string");
            return;
        }
        emit(AsmToken::Assert, 1);
    } else {
        emit(AsmToken::Assert, 0);
    }
}

// =============================================================================
// Phase 9 — Constant folding (peephole optimizer)
// =============================================================================

void Parser::peephole_fold() noexcept {
    const int sz = static_cast<int>(prog_.code.size());
    if (sz < 2) return;
    auto& code   = prog_.code;
    auto& srcmap = prog_.src_map;

    // Unary: PushC + Inv → PushC(-val)
    if (code[static_cast<std::size_t>(sz-1)].token == AsmToken::Inv) {
        if (sz >= 2 && code[static_cast<std::size_t>(sz-2)].token == AsmToken::PushC) {
            code[static_cast<std::size_t>(sz-2)].n = -code[static_cast<std::size_t>(sz-2)].n;
            code.pop_back(); srcmap.pop_back();
        }
        return;
    }

    // Binary: PushC + PushC + op → PushC(result)
    if (sz < 3) return;
    if (code[static_cast<std::size_t>(sz-2)].token != AsmToken::PushC) return;
    if (code[static_cast<std::size_t>(sz-3)].token != AsmToken::PushC) return;

    const double a = code[static_cast<std::size_t>(sz-3)].n;
    const double b = code[static_cast<std::size_t>(sz-2)].n;
    const AsmToken op = code[static_cast<std::size_t>(sz-1)].token;
    double result = 0.0;

    switch (op) {
        case AsmToken::Add: result = a + b; break;
        case AsmToken::Sub: result = a - b; break;
        case AsmToken::Mul: result = a * b; break;
        case AsmToken::Div:
            if (b == 0.0) return;
            result = a / b; break;
        case AsmToken::Mod:
            if (b == 0.0) return;
            result = std::fmod(a, b); break;
        case AsmToken::Pow:
            if (a < 0.0 && b != std::trunc(b)) return;
            result = std::pow(a, b); break;
        default: return;
    }

    code.pop_back(); code.pop_back();
    srcmap.pop_back(); srcmap.pop_back();
    code.back().token = AsmToken::PushC;
    code.back().i     = 0;
    code.back().n     = result;
}

} // namespace p9b
