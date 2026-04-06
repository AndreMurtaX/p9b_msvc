// =============================================================================
// lexer.h  –  Plan9Basic C++ port: BASIC source tokenizer
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
// License : MIT  –  same as the original Pascal source
//
// Mirrors TBasicLexer in lexer.pas.
// Breaks a null-terminated BASIC source string into a flat array of BasInstr.
// The source pointer must remain valid for the lifetime of this object.
// =============================================================================
#pragma once

#include "p9b_types.h"
#include <string>
#include <vector>
#include <cstdint>

namespace p9b {

class BasicLexer {
public:
    BasicLexer()  = default;
    ~BasicLexer() = default;

    // Non-copyable (owns internal state tied to source pointer)
    BasicLexer(const BasicLexer&)            = delete;
    BasicLexer& operator=(const BasicLexer&) = delete;

    // Movable
    BasicLexer(BasicLexer&&)                 = default;
    BasicLexer& operator=(BasicLexer&&)      = default;

    // ── Main entry point ────────────────────────────────────────────────────
    // Tokenise the entire BASIC program held in 'source'.
    // 'source' must be a null-terminated UTF-8 / ASCII string.
    // The pointer must stay alive as long as currS() / tokenInfo() are used.
    void loadProg(const char* source);

    // ── Cursor navigation ───────────────────────────────────────────────────
    void advance();              // move IP to the next token
    void putBack();              // move IP to the previous token
    void gotoToken(int n);       // jump to token index n

    // ── Current-token accessors ─────────────────────────────────────────────
    // currS  – string slice of the current token in the original source
    // currN  – numeric value (for integer/float literals) or keyword hash-code
    // currPos– byte offset of the current token inside the source string
    [[nodiscard]] std::string currS()   const;
    [[nodiscard]] double      currN()   const noexcept;
    [[nodiscard]] int         currPos() const noexcept;
    [[nodiscard]] BasToken    currTok() const noexcept;
    [[nodiscard]] BasToken    prevTok() const noexcept;
    [[nodiscard]] BasToken    nextTok() const noexcept;

    // Return full descriptor for token at absolute index n
    [[nodiscard]] BasInstr tokenInfo(int n) const noexcept;

    // Total raw tokens scanned (including whitespace-skipped / comment tokens)
    [[nodiscard]] std::int64_t totalTokens() const noexcept;

    // ── Error state ─────────────────────────────────────────────────────────
    [[nodiscard]] bool               hasError()     const noexcept { return error_;    }
    [[nodiscard]] const std::string& errorMessage() const noexcept { return errorMsg_; }

    // ── Current instruction pointer (absolute index) ─────────────────────────
    [[nodiscard]] int currIP() const noexcept { return ip_; }

    // ── Source position helpers ──────────────────────────────────────────────
    // Convert a byte offset (from currPos / BasInstr::pos) to 1-based line + col.
    [[nodiscard]] std::pair<int,int> pos_to_line_col(int pos) const noexcept;

private:
    // Source pointer (not owned)
    const char*           source_   = nullptr;

    // Tokenised program: prog_[0] = sentinel CRLF, prog_[1..n] = real tokens
    std::vector<BasInstr> prog_;

    // Cursor state
    int           ip_       = 0;  // current token index
    int           idx_      = 0;  // current byte position in source_

    // Statistics
    std::uint32_t tokCount_ = 0;

    // Error state
    bool          error_    = false;
    std::string   errorMsg_;

    // ── Internal tokeniser ───────────────────────────────────────────────────
    void     basGetToken(std::string& tokenStr,
                         int&         tokenPos,
                         int&         tokenLen,
                         BasToken&    tok);

    [[nodiscard]] BasToken basIdentKind(const std::string& tokStr);

    // ── Static character-classification helpers ──────────────────────────────
    // All operate on plain char; cast to unsigned char before any stdlib call.

    [[nodiscard]] static bool isBlank(char c) noexcept;
    [[nodiscard]] static bool isIdentStart(char c) noexcept;
    [[nodiscard]] static bool isValidIdentChar(char c) noexcept;
    [[nodiscard]] static bool isDigit(char c) noexcept;
    [[nodiscard]] static bool isTypeChar(char c) noexcept;

    // Convert a single escape character to the corresponding string
    [[nodiscard]] static std::string convertEscape(char c);

    // Validate that a complete identifier token is well-formed
    [[nodiscard]] static bool validIdentifier(const std::string& token);
};

} // namespace p9b
