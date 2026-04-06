// =============================================================================
// lexer.cpp  –  Plan9Basic C++ port: BASIC source tokenizer
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
// License : MIT  –  same as the original Pascal source
//
// Direct translation of TBasicLexer (lexer.pas).
// All logic is preserved; Pascal-specific constructs are replaced with their
// nearest C++23 equivalents.
// =============================================================================

#include "lexer.h"
#include "p9b_utils.h"

#include <cctype>
#include <stdexcept>

namespace p9b {

// =============================================================================
// Static character-classification helpers
// =============================================================================

// Whitespace that is silently skipped (not a newline)
// Mirrors: IsInArray([#8,#9,#32]) in lexer.pas
bool BasicLexer::isBlank(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\b';   // #32 #9 #8
}

// Valid first character of an identifier or keyword  (A-Z a-z _)
// Mirrors: 'A'..'Z', 'a'..'z', '_'  in the case statement
bool BasicLexer::isIdentStart(char c) noexcept {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

// Valid continuation character of an identifier
// validIdentChars = A-Z a-z 0-9 _ $ # :
bool BasicLexer::isValidIdentChar(char c) noexcept {
    return std::isalnum(static_cast<unsigned char>(c))
        || c == '_' || c == '$' || c == '#' || c == ':';
}

bool BasicLexer::isDigit(char c) noexcept {
    return c >= '0' && c <= '9';
}

// typeChars = $ or #  (suffix that turns an identifier into a typed variable)
bool BasicLexer::isTypeChar(char c) noexcept {
    return c == '$' || c == '#';
}

// Convert the character immediately after a backslash to its escape value.
// Mirrors ConvertEscapeSequence in lexer.pas.
std::string BasicLexer::convertEscape(char c) {
    switch (c) {
        case '"':  return "\"";
        case '\\': return "\\";
        case 'n':  return "\n";
        case 'r':  return "\r";
        case 't':  return "\t";
        case '0':  return std::string(1, '\0');
        case 'b':  return "\b";
        case 'f':  return "\f";
        case 'v':  return "\v";
        case 'a':  return "\a";
        default:   return std::string("\\") + c;  // unknown: keep as-is
    }
}

// An identifier is well-formed if:
//   – its first character (uppercased) is alpha or '_'
//   – any type-suffix character ($ #) appears only as the last character
// Mirrors ValidIdentifier in lexer.pas.
bool BasicLexer::validIdentifier(const std::string& token) {
    if (token.empty()) return false;

    const unsigned char first = static_cast<unsigned char>(token[0]);
    if (!std::isalpha(first) && token[0] != '_')
        return false;

    bool endToken = false;
    for (std::size_t i = 1; i < token.size(); ++i) {
        if (isTypeChar(token[i]))
            endToken = true;
        if (endToken && i < token.size() - 1)
            return false;   // type suffix not at end
    }
    return true;
}

// =============================================================================
// loadProg
// =============================================================================
// Tokenise the complete BASIC source and populate prog_.
// Mirrors TBasicLexer.LoadProg.
void BasicLexer::loadProg(const char* source) {
    source_   = source;
    idx_      = 0;
    ip_       = 1;    // prog_[0] is the sentinel CRLF; real tokens start at 1
    tokCount_ = 0;
    error_    = false;
    errorMsg_.clear();

    prog_.resize(static_cast<std::size_t>(INITINSTRSIZE) + 1);
    int capacity = INITINSTRSIZE;

    bool skipComments = false;

    do {
        std::string data;
        int  tokPos = 0, tokLen = 0;
        BasToken id = BasToken::None;

        basGetToken(data, tokPos, tokLen, id);
        ++tokCount_;

        // A REM token (or ') starts a line comment; skip until CRLF
        if (id == BasToken::Rem)
            skipComments = true;

        if (skipComments) {
            if (id == BasToken::CRLF)
                skipComments = false;   // CRLF ends the comment
            else
                continue;               // discard comment body
        }

        // Store the instruction
        BasInstr& instr = prog_[ip_];
        instr.id  = id;
        instr.pos = tokPos;
        instr.len = tokLen;

        if (id == BasToken::Integer || id == BasToken::Float || id == BasToken::Hash) {
            bool ok = false;
            instr.n = utils::str_to_double(data, ok);
            if (!ok) instr.id = BasToken::Unknown;
        } else {
            // For non-numeric tokens: store the keyword hash-code so the
            // parser can do fast O(1) comparisons without re-hashing.
            instr.n = static_cast<double>(utils::string_code(utils::upper(data)));
        }

        ++ip_;

        // Grow the buffer when full (double each time, mirrors SetLength x2)
        if (ip_ == capacity) {
            capacity *= 2;
            prog_.resize(static_cast<std::size_t>(capacity) + 1);
        }

    } while (prog_[ip_ - 1].id != BasToken::Null
          && ip_ < MAXINSTR
          && !error_);

    // Write the null-terminator instruction
    prog_[ip_].id  = BasToken::Null;
    prog_[ip_].pos = (ip_ > 0) ? prog_[ip_ - 1].pos : 0;

    // Trim excess allocation
    prog_.resize(static_cast<std::size_t>(ip_) + 1);
    prog_.shrink_to_fit();

    // Reset cursor to position 0; prog_[0] is a synthetic CRLF sentinel
    ip_        = 0;
    prog_[0].id = BasToken::CRLF;
}

// =============================================================================
// basGetToken
// =============================================================================
// Read the next token from source_ starting at idx_.
// On return, tokenStr holds the raw text, tokenPos/tokenLen the position, and
// tok the token kind.
// Mirrors TBasicLexer.BasGetToken (the inner procedure in lexer.pas).
void BasicLexer::basGetToken(std::string& tokenStr,
                              int&         tokenPos,
                              int&         tokenLen,
                              BasToken&    tok)
{
    // Skip horizontal whitespace (not newlines – those are tokens)
    while (source_[idx_] && isBlank(source_[idx_]))
        ++idx_;

    tokenLen = 1;
    tokenPos = idx_;
    const char c = source_[idx_];

    // ── Identifier / keyword ─────────────────────────────────────────────────
    if (isIdentStart(c)) {
        tokenPos = idx_++;
        while (source_[idx_] && isValidIdentChar(source_[idx_]))
            ++idx_;

        tokenLen = idx_ - tokenPos;
        tokenStr.assign(source_ + tokenPos, static_cast<std::size_t>(tokenLen));

        // The last character determines the base type
        const char last = source_[idx_ - 1];
        if      (last == '$') tok = BasToken::StrIdentifier;
        else if (last == '#') tok = BasToken::PointerIdentifier;
        else if (last == ':') tok = BasToken::Label;
        else                  tok = basIdentKind(tokenStr);

        // Skip trailing blanks before looking for ( or [
        while (source_[idx_] && isBlank(source_[idx_]))
            ++idx_;

        if (source_[idx_] == '(') {
            ++idx_;     // consume '('
            switch (tok) {
                case BasToken::Identifier:
                    tok = BasToken::NumFunction;     return;
                case BasToken::PointerIdentifier:
                    tok = BasToken::PointerFunction; return;
                case BasToken::StrIdentifier:
                    tok = BasToken::StrFunction;     return;
                default: break;
            }
        } else if (source_[idx_] == '[') {
            switch (tok) {
                case BasToken::StrIdentifier:
                    if (source_[idx_ - 2] == '#') {
                        ++idx_; tok = BasToken::PointerArrayStr;
                    } else if (source_[idx_ + 1] == '[') {
                        idx_ += 2; tok = BasToken::CharArray;
                    } else {
                        ++idx_; tok = BasToken::StrArray;
                    }
                    return;
                case BasToken::PointerIdentifier:
                    if (source_[idx_ - 2] == '#') {
                        ++idx_; tok = BasToken::PointerArrayPtr;
                    } else {
                        ++idx_; tok = BasToken::PointerArray;
                    }
                    return;
                default:
                    ++idx_; tok = BasToken::Unknown;
                    return;
            }
        } else {
            if (!validIdentifier(tokenStr))
                tok = BasToken::Unknown;
        }
        return;
    }

    // ── Numeric literal ──────────────────────────────────────────────────────
    if (isDigit(c) || c == '.') {
        tokenPos = idx_++;
        tok = BasToken::Integer;

        while (source_[idx_] && (isDigit(source_[idx_]) || source_[idx_] == '.')) {
            if (source_[idx_] == '.') tok = BasToken::Float;
            ++idx_;
        }

        // Scientific notation: e / E, optional +/-, mandatory digits
        if (source_[idx_] == 'e' || source_[idx_] == 'E') {
            tok = BasToken::Float;
            ++idx_;
            if (source_[idx_] == '+' || source_[idx_] == '-') ++idx_;
            if (!isDigit(source_[idx_])) {
                tok = BasToken::Unknown;
            } else {
                while (isDigit(source_[idx_])) ++idx_;
            }
        }

        tokenLen = idx_ - tokenPos;
        tokenStr.assign(source_ + tokenPos, static_cast<std::size_t>(tokenLen));

        // Numbers starting with '.'  →  prepend "0" so from_chars can parse
        if (source_[tokenPos] == '.') {
            tok = BasToken::Float;
            tokenStr = "0" + tokenStr;
        }

        if (tok != BasToken::Unknown) {
            bool ok = false;
            double d = utils::str_to_double(tokenStr, ok);
            if (!ok)
                tok = BasToken::Unknown;
            else if (tok == BasToken::Integer && d > MAX_INTEGER_VALUE)
                tok = BasToken::Float;
        }

        // Optional 'b'/'B' suffix on integer literals (byte range 0-255)
        if (tok == BasToken::Integer
                && (source_[idx_] == 'b' || source_[idx_] == 'B')) {
            ++idx_;     // consume suffix; value was already validated above
        }
        return;
    }

    // ── Line terminators ─────────────────────────────────────────────────────
    if (c == '\n') {        // LF  (Unix)
        tok      = BasToken::CRLF;
        tokenStr = "\n";
        tokenPos = idx_++;
        return;
    }

    if (c == '\r') {        // CR or CR+LF  (Windows / classic Mac)
        tok      = BasToken::CRLF;
        tokenStr = "\n";
        tokenPos = idx_++;
        if (source_[idx_] == '\n') ++idx_;   // consume LF in CRLF pair
        return;
    }

    // ── String literal ───────────────────────────────────────────────────────
    if (c == '"') {
        tok = BasToken::String;
        tokenStr.clear();
        bool isEscaped = false;

        // The loop mirrors Pascal's repeat…until exactly:
        //   body processes source_[idx_], then Inc(idx_) at bottom.
        //   Loop exits when  (!isEscaped && source_[idx_] == '"').
        //   On error (NUL/CR/LF) we Dec(idx_) and break, then ++idx_ below.
        do {
            const char sc = source_[idx_];

            if (sc == '\0' || sc == '\n' || sc == '\r') {
                --idx_;                         // undo to stay at error char
                tok = BasToken::Unknown;
                break;                          // skips ++idx_ at bottom
            }

            if (sc == '\\') {
                if (isEscaped) {
                    tokenStr += '\\';
                    isEscaped = false;
                } else {
                    isEscaped = true;
                }
            } else {
                if (isEscaped) {
                    tokenStr += convertEscape(sc);
                    isEscaped = false;
                } else if (sc != '"') {
                    tokenStr += sc;
                }
            }

            ++idx_;     // Inc(idx) at the bottom of the Pascal loop

        } while (isEscaped || source_[idx_] != '"');

        ++idx_;         // mirror: Pascal's unconditional Inc(idx) after the loop
                        // → skips closing '"' on success, restores on error

        tokenPos = tokenPos + 1;                // skip past opening '"'
        tokenLen = idx_ - tokenPos - 1;
        return;
    }

    // ── End of source ────────────────────────────────────────────────────────
    if (c == '\0') {
        tok      = BasToken::Null;
        tokenStr.clear();
        tokenPos = idx_;
        return;
    }

    // ── Symbols ──────────────────────────────────────────────────────────────
    {
        tokenPos = idx_;
        switch (c) {
            case '\'': tok = BasToken::Rem;        break;
            case '|':  tok = BasToken::Pipe;       break;
            case '@':  tok = BasToken::At;         break;

            case '&':
                if      (source_[idx_ + 1] == '#') { ++idx_; tok = BasToken::IndirectCallPtr; }
                else if (source_[idx_ + 1] == '$') { ++idx_; tok = BasToken::IndirectCallStr; }
                else                                          tok = BasToken::Ampersand;
                break;

            case '=':  tok = BasToken::Equal;      break;
            case '(':  tok = BasToken::RoundOpen;  break;
            case ')':  tok = BasToken::RoundClose; break;
            case '[':  tok = BasToken::SquareOpen; break;

            case ']':
                if (source_[idx_ + 1] == ']') { ++idx_; tok = BasToken::DoubleSquareClose; }
                else                                     tok = BasToken::SquareClose;
                break;

            case '{':  tok = BasToken::CurlyOpen;  break;
            case '}':  tok = BasToken::CurlyClose; break;
            case ',':  tok = BasToken::Comma;      break;
            case ';':  tok = BasToken::SemiColon;  break;
            case '*':  tok = BasToken::Star;       break;
            case '/':  tok = BasToken::Slash;      break;
            case '+':  tok = BasToken::Plus;       break;
            case ':':  tok = BasToken::Colon;      break;
            case '^':  tok = BasToken::Power;      break;
            case '-':  tok = BasToken::Minus;      break;

            // '#' followed by digits → file-number token (e.g. #1, #12).
            // tokenStr will hold just the digit string so loadProg() can convert it
            // to a numeric value with str_to_double(), exactly like Integer tokens.
            case '#':
                if (isDigit(source_[idx_ + 1])) {
                    tokenPos = idx_ + 1;     // digit(s) start after '#'
                    idx_ += 1;               // skip '#'; loop below will be offset-by-1
                    while (isDigit(source_[idx_ + 1])) ++idx_;  // scan remaining digits
                    tok = BasToken::Hash;
                    // The generic ++idx_ and tokenStr.assign() below will then capture
                    // exactly the digit portion: idx_ points at last digit, +1 lands
                    // past it, tokenLen = new_idx - tokenPos = digit count.
                } else {
                    tok = BasToken::Symbol;
                }
                break;

            case '?':
                if      (source_[idx_ + 1] == '>') { ++idx_; tok = BasToken::Max; }
                else if (source_[idx_ + 1] == '<') { ++idx_; tok = BasToken::Min; }
                else                                          tok = BasToken::Symbol;
                break;

            case '<':
                if      (source_[idx_ + 1] == '=') { ++idx_; tok = BasToken::LowerEqual; }
                else if (source_[idx_ + 1] == '>') { ++idx_; tok = BasToken::NotEqual;   }
                else                                          tok = BasToken::Lower;
                break;

            case '>':
                if (source_[idx_ + 1] == '=') { ++idx_; tok = BasToken::GreaterEqual; }
                else                                     tok = BasToken::Greater;
                break;

            default:
                tok = BasToken::Symbol;
                break;
        }

        ++idx_;
        tokenLen = idx_ - tokenPos;
        tokenStr.assign(source_ + tokenPos, static_cast<std::size_t>(tokenLen));
    }
}

// =============================================================================
// basIdentKind
// =============================================================================
// Distinguish keywords from plain identifiers using the same hash-table lookup
// as the original Pascal implementation (hash = sum of uppercase char codes).
// Mirrors TBasicLexer.BasIdentKind.
BasToken BasicLexer::basIdentKind(const std::string& tokStr) {
    const std::string upper = utils::upper(tokStr);
    const int hash = utils::string_code(upper);

    // Fast early-out: all keywords fall in [143, 829]
    if (hash < 143 || hash > 829) return BasToken::Identifier;

    switch (hash) {
        case 143: if (upper == "IF")          return BasToken::If;
                  break;
        case 147: if (upper == "DO")          return BasToken::Do;
                  break;
        case 148: if (upper == "AS")          return BasToken::As;
                  break;
        // ERL = 69+82+76 = 227
        case 227: if (upper == "ERL")         return BasToken::Erl;
                  break;
        // ERR = 69+82+82 = 233
        case 233: if (upper == "ERR")         return BasToken::Err;
                  break;
        case 157: if (upper == "ON")          return BasToken::On;
                  break;
        case 161: if (upper == "OR")          return BasToken::Or;
                  break;
        case 163: if (upper == "TO")          return BasToken::To;
                  break;
        case 211: if (upper == "AND")         return BasToken::And;
                  break;
        case 215: if (upper == "END")         return BasToken::End;
                  break;
        case 224: if (upper == "MOD")         return BasToken::Mod;
                  break;
        case 226: if (upper == "CLS")         return BasToken::Cls;
                  break;
        case 228: if (upper == "REM")         return BasToken::Rem;
                  break;
        case 229: if (upper == "LET")         return BasToken::Let;
                  break;
        case 231: if (upper == "FOR")         return BasToken::For;
                  break;
        case 241: if (upper == "NOT")         return BasToken::Not;
                  break;
        case 282: if (upper == "DATA")        return BasToken::Data;
                  break;
        case 296: if (upper == "LINE")        return BasToken::Line;
                  break;
        case 306: if (upper == "OPEN")        return BasToken::Open;
                  break;
        case 284:
            if (upper == "CALL") return BasToken::Call;
            if (upper == "CASE") return BasToken::Case;
            if (upper == "READ") return BasToken::Read;
            break;
        case 297: if (upper == "ELSE")        return BasToken::Else;
                  break;
        case 302: if (upper == "WEND")        return BasToken::EndWhile;
                  break;
        case 303: if (upper == "THEN")        return BasToken::Then;
                  break;
        case 310: if (upper == "DUMP")        return BasToken::Dump;
                  break;
        case 313: if (upper == "GOTO")        return BasToken::Goto;
                  break;
        case 314: if (upper == "LOOP")        return BasToken::Loop;
                  break;
        case 315: if (upper == "NULL")        return BasToken::JsonNull;
                  break;
        case 316: if (upper == "STEP")        return BasToken::Step;
                  break;
        case 319: if (upper == "NEXT")        return BasToken::Next;
                  break;
        case 320: if (upper == "TRUE")        return BasToken::True;
                  break;
        case 357: if (upper == "BREAK")       return BasToken::Break;
                  break;
        case 374: if (upper == "CLOSE")       return BasToken::Close;
                  break;
        // SHELL = 83+72+69+76+76 = 376
        case 376: if (upper == "SHELL")       return BasToken::Shell;
                  break;
        case 358: if (upper == "ENDIF")       return BasToken::EndIf;
                  break;
        case 363:
            if (upper == "LOCAL") return BasToken::Local;
            if (upper == "FALSE") return BasToken::False;
            break;
        case 367: if (upper == "TRACE")       return BasToken::Trace;
                  break;
        case 375: if (upper == "WATCH")       return BasToken::Watch;
                  break;
        case 377: if (upper == "WHILE")       return BasToken::While;
                  break;
        case 383: if (upper == "COLOR")       return BasToken::Color;
                  break;
        case 384: if (upper == "GOSUB")       return BasToken::Gosub;
                  break;
        case 385: if (upper == "TIMER")       return BasToken::Timer;
                  break;
        case 396: if (upper == "UNTIL")       return BasToken::Until;
                  break;
        case 397: if (upper == "PRINT")       return BasToken::Print;
                  break;
        case 400: if (upper == "INPUT")       return BasToken::Input;
                  break;
        case 440:
            if (upper == "LOCATE") return BasToken::Locate;
            if (upper == "APPEND") return BasToken::Append;
            break;
        // USING = 85+83+73+78+71 = 390
        case 390: if (upper == "USING")       return BasToken::Using;
                  break;
        // ERROR = 69+82+82+79+82 = 394
        case 394: if (upper == "ERROR")       return BasToken::Error;
                  break;
        // WRITE = 87+82+73+84+69 = 395
        case 395: if (upper == "WRITE")       return BasToken::Write;
                  break;
        case 446: if (upper == "ENDFOR")      return BasToken::EndFor;
                  break;
        case 448: if (upper == "SELECT")      return BasToken::Select;
                  break;
        case 449: if (upper == "REPEAT")      return BasToken::Repeat;
                  break;
        case 466: if (upper == "ASSERT")      return BasToken::Assert;
                  break;
        case 465: if (upper == "RESUME")       return BasToken::Resume;
                  break;
        case 480: if (upper == "RETURN")      return BasToken::Return;
                  break;
        case 497: if (upper == "OUTPUT")      return BasToken::Output;
                  break;
        case 524: if (upper == "TRACEON")     return BasToken::TraceOn;
                  break;
        case 538: if (upper == "UNWATCH")     return BasToken::Unwatch;
                  break;
        case 548: if (upper == "RESTORE")     return BasToken::Restore;
                  break;
        case 551: if (upper == "PRINTLN")     return BasToken::PrintLn;
                  break;
        case 586: if (upper == "TRACEOFF")    return BasToken::TraceOff;
                  break;
        case 592: if (upper == "ENDWHILE")    return BasToken::EndWhile;
                  break;
        case 613: if (upper == "CONTINUE")    return BasToken::Continue;
                  break;
        case 614: if (upper == "FUNCTION")    return BasToken::Function;
                  break;
        case 663: if (upper == "ENDSELECT")   return BasToken::EndSelect;
                  break;
        case 751: if (upper == "BREAKPOINT")  return BasToken::BreakPoint;
                  break;
        case 827: if (upper == "REFRESHRATE") return BasToken::RefreshRate;
                  break;
        case 829: if (upper == "ENDFUNCTION") return BasToken::EndFunction;
                  break;
        default:  break;
    }
    return BasToken::Identifier;
}

// =============================================================================
// Cursor navigation
// =============================================================================

void BasicLexer::advance() {
    ++ip_;
}

void BasicLexer::putBack() {
    if (ip_ > 0) --ip_;
}

void BasicLexer::gotoToken(int n) {
    if (n >= 0) ip_ = n;
}

// =============================================================================
// Accessors
// =============================================================================

std::string BasicLexer::currS() const {
    if (ip_ < 0 || ip_ >= static_cast<int>(prog_.size())) return {};
    const BasInstr& instr = prog_[ip_];
    if (!source_ || instr.len <= 0) return {};
    return {source_ + instr.pos, static_cast<std::size_t>(instr.len)};
}

double BasicLexer::currN() const noexcept {
    if (ip_ < 0 || ip_ >= static_cast<int>(prog_.size())) return 0.0;
    return prog_[ip_].n;
}

int BasicLexer::currPos() const noexcept {
    if (ip_ < 0 || ip_ >= static_cast<int>(prog_.size())) return 0;
    return prog_[ip_].pos;
}

BasToken BasicLexer::currTok() const noexcept {
    if (ip_ < 0 || ip_ >= static_cast<int>(prog_.size())) return BasToken::None;
    return prog_[ip_].id;
}

BasToken BasicLexer::prevTok() const noexcept {
    if (ip_ <= 0) return BasToken::None;
    return prog_[ip_ - 1].id;
}

BasToken BasicLexer::nextTok() const noexcept {
    const int next = ip_ + 1;
    if (next >= static_cast<int>(prog_.size())) return BasToken::None;
    return prog_[next].id;
}

BasInstr BasicLexer::tokenInfo(int n) const noexcept {
    if (n < 0 || n >= static_cast<int>(prog_.size())) return {};
    return prog_[n];
}

std::int64_t BasicLexer::totalTokens() const noexcept {
    return static_cast<std::int64_t>(tokCount_);
}

// =============================================================================
// Source position → (line, col), both 1-based.
// Scans forward through source_ counting newlines up to the given byte offset.
// =============================================================================
std::pair<int,int> BasicLexer::pos_to_line_col(int pos) const noexcept {
    if (!source_ || pos <= 0) return {1, 1};
    int line = 1, col = 1;
    for (int i = 0; i < pos && source_[i] != '\0'; ++i) {
        if (source_[i] == '\n') { ++line; col = 1; }
        else                    { ++col;            }
    }
    return {line, col};
}

} // namespace p9b
