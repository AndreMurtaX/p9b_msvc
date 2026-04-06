// =============================================================================
// main.cpp  –  Plan9Basic command-line interpreter  (p9b)
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
//
// Usage:
//   p9b                      Interactive REPL (syntax checker)
//   p9b <file.bas>           Tokenize file; report errors and stats
//   p9b --tokens <file.bas>  Tokenize file and dump the full token stream
//   p9b --funcs              List all registered built-in functions
//   p9b --help               Show this help
//
// REPL commands (prefix with '.'):
//   .help    Show help
//   .funcs   List built-in functions
//   .check   Tokenize the current program buffer and show results
//   .show    Print the current program buffer
//   .clear   Clear the program buffer
//   .tokens  Toggle per-line token display  (default: off)
//   .quit    Exit
// =============================================================================

#include "basic.h"       // engine orchestrator (lexer + parser + exec)
#include "lexer.h"       // engine/lexer.h  (via p9b_engine include dir)
#include "p9b_types.h"   // engine/p9b_types.h
#include "p9b_utils.h"   // engine/p9b_utils.h
#include "num_lib.h"     // libs/num_lib.h   (via p9b_numlib include dir)
#include "str_lib.h"     // libs/str_lib.h   (via p9b_strlib include dir)

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

// =============================================================================
// Token display helpers
// =============================================================================

static std::string_view tok_name(p9b::BasToken t) noexcept {
    using T = p9b::BasToken;
    switch (t) {
        case T::And:               return "AND";
        case T::Ampersand:         return "&";
        case T::Assert:            return "ASSERT";
        case T::At:                return "@";
        case T::Break:             return "BREAK";
        case T::BreakPoint:        return "BREAKPOINT";
        case T::Call:              return "CALL";
        case T::Case:              return "CASE";
        case T::CharArray:         return "CharArray";
        case T::CRLF:              return "CRLF";
        case T::Cls:               return "CLS";
        case T::Colon:             return ":";
        case T::Color:             return "COLOR";
        case T::Comma:             return ",";
        case T::Continue:          return "CONTINUE";
        case T::CurlyClose:        return "}";
        case T::CurlyOpen:         return "{";
        case T::Data:              return "DATA";
        case T::Do:                return "DO";
        case T::DoubleSquareClose: return "]]";
        case T::DoubleSquareOpen:  return "[[";
        case T::Dump:              return "DUMP";
        case T::Else:              return "ELSE";
        case T::End:               return "END";
        case T::EndFor:            return "ENDFOR";
        case T::EndFunction:       return "ENDFUNCTION";
        case T::EndIf:             return "ENDIF";
        case T::EndSelect:         return "ENDSELECT";
        case T::EndWhile:          return "ENDWHILE";
        case T::Equal:             return "=";
        case T::False:             return "FALSE";
        case T::Float:             return "Float";
        case T::For:               return "FOR";
        case T::Function:          return "FUNCTION";
        case T::Gosub:             return "GOSUB";
        case T::Goto:              return "GOTO";
        case T::Greater:           return ">";
        case T::GreaterEqual:      return ">=";
        case T::Identifier:        return "Identifier";
        case T::If:                return "IF";
        case T::IndirectCallPtr:   return "&#";
        case T::IndirectCallStr:   return "&$";
        case T::Input:             return "INPUT";
        case T::Integer:           return "Integer";
        case T::JsonNull:          return "NULL";
        case T::Label:             return "Label";
        case T::Let:               return "LET";
        case T::Local:             return "LOCAL";
        case T::Locate:            return "LOCATE";
        case T::Loop:              return "LOOP";
        case T::Lower:             return "<";
        case T::LowerEqual:        return "<=";
        case T::Max:               return "?>";
        case T::Min:               return "?<";
        case T::Minus:             return "-";
        case T::Mod:               return "MOD";
        case T::Next:              return "NEXT";
        case T::None:              return "(none)";
        case T::Not:               return "NOT";
        case T::NotEqual:          return "<>";
        case T::Null:              return "(null)";
        case T::NumFunction:       return "NumFunction";
        case T::On:                return "ON";
        case T::Or:                return "OR";
        case T::Pipe:              return "|";
        case T::Plus:              return "+";
        case T::PointerArray:      return "PointerArray";
        case T::PointerArrayStr:   return "PointerArrayStr";
        case T::PointerArrayPtr:   return "PointerArrayPtr";
        case T::PointerFunction:   return "PointerFunction";
        case T::PointerIdentifier: return "PointerIdentifier";
        case T::Power:             return "^";
        case T::Print:             return "PRINT";
        case T::PrintLn:           return "PRINTLN";
        case T::Read:              return "READ";
        case T::RefreshRate:       return "REFRESHRATE";
        case T::Rem:               return "REM";
        case T::Repeat:            return "REPEAT";
        case T::Restore:           return "RESTORE";
        case T::Return:            return "RETURN";
        case T::RoundClose:        return ")";
        case T::RoundOpen:         return "(";
        case T::Select:            return "SELECT";
        case T::SemiColon:         return ";";
        case T::Slash:             return "/";
        case T::SquareClose:       return "]";
        case T::SquareOpen:        return "[";
        case T::Star:              return "*";
        case T::Step:              return "STEP";
        case T::StrArray:          return "StrArray";
        case T::StrFunction:       return "StrFunction";
        case T::StrIdentifier:     return "StrIdentifier";
        case T::String:            return "String";
        case T::Symbol:            return "Symbol";
        case T::Then:              return "THEN";
        case T::Timer:             return "TIMER";
        case T::To:                return "TO";
        case T::Trace:             return "TRACE";
        case T::TraceOff:          return "TRACEOFF";
        case T::TraceOn:           return "TRACEON";
        case T::True:              return "TRUE";
        case T::Unknown:           return "(unknown)";
        case T::Until:             return "UNTIL";
        case T::Unwatch:           return "UNWATCH";
        case T::Watch:             return "WATCH";
        case T::While:             return "WHILE";
        default:                   return "(?)";
    }
}

// Build the display value for a token: either its source text or a description.
static std::string tok_value(p9b::BasToken id, std::string_view src_text, double n) {
    using T = p9b::BasToken;
    switch (id) {
        case T::CRLF:   return "(newline)";
        case T::Null:   return "(end of source)";
        case T::None:   return "(sentinel)";
        case T::Integer:
        case T::Float:
            return std::format("{} = {}", src_text, n);
        case T::String:
            return std::format("\"{}\"", src_text);
        default:
            return std::string{ src_text };
    }
}

// =============================================================================
// Token stream dump
// Iterates all stored tokens and prints a formatted table.
// =============================================================================
static int dump_token_stream(p9b::BasicLexer& lex, const char* src) {
    // Build line-start offset table for fast line-number lookup.
    std::vector<int> line_starts{ 0 };
    for (int i = 0; src[i] != '\0'; ++i)
        if (src[i] == '\n')
            line_starts.push_back(i + 1);

    auto line_of = [&](int pos) -> int {
        const auto it = std::upper_bound(line_starts.begin(), line_starts.end(), pos);
        return static_cast<int>(std::distance(line_starts.begin(), it));
    };

    std::cout << std::format("{:>5}  {:<20}  {:<35}  {}\n",
        "#", "Token", "Value", "Line:Pos");
    std::cout << std::string(70, '-') << '\n';

    lex.gotoToken(1); // skip sentinel at index 0
    int idx = 1;
    while (lex.currTok() != p9b::BasToken::Null) {
        const p9b::BasInstr info     = lex.tokenInfo(lex.currIP());
        const std::string   src_text = lex.currS();
        const std::string   value    = tok_value(info.id, src_text, info.n);
        const int           line     = line_of(info.pos);

        std::cout << std::format("{:>5}  {:<20}  {:<35}  {}:{}\n",
            idx,
            tok_name(info.id),
            value.size() <= 35 ? value : value.substr(0, 32) + "...",
            line,
            info.pos);

        lex.advance();
        ++idx;
    }
    std::cout << std::string(70, '-') << '\n';
    return idx - 1; // stored token count
}

// =============================================================================
// INCLUDE preprocessor
// Expands INCLUDE "filename" directives inline before parsing.
// =============================================================================
static std::string preprocess(const std::string& source,
                               const std::filesystem::path& base_dir,
                               int depth = 0) {
    if (depth > 16) return "' INCLUDE: max depth exceeded\n";
    std::string result;
    result.reserve(source.size());
    std::istringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) {
        // Trim leading whitespace to detect INCLUDE
        std::size_t s = line.find_first_not_of(" \t");
        std::string trimmed = (s == std::string::npos) ? "" : line.substr(s);
        std::string upper;
        upper.reserve(trimmed.size());
        for (char c : trimmed) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (upper.starts_with("INCLUDE")) {
            auto q1 = trimmed.find('"');
            auto q2 = trimmed.rfind('"');
            if (q1 != std::string::npos && q2 != q1) {
                std::string fname = trimmed.substr(q1 + 1, q2 - q1 - 1);
                auto full = base_dir / fname;
                std::ifstream f(full);
                if (f.is_open()) {
                    std::string sub((std::istreambuf_iterator<char>(f)), {});
                    result += preprocess(sub, full.parent_path(), depth + 1);
                    result += '\n';
                    continue;
                } else {
                    result += "' INCLUDE ERROR: cannot open " + fname + "\n";
                    continue;
                }
            }
        }
        result += line + '\n';
    }
    return result;
}

// =============================================================================
// File loading
// =============================================================================
static bool load_file(const std::string& path, std::string& out_src) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out_src = ss.str();
    return true;
}

// =============================================================================
// Tokenize a source string and report results.
// Returns the number of errors found (0 = ok).
// =============================================================================
static int tokenize_and_report(const std::string& src,
                                const std::string& label,
                                bool               show_tokens)
{
    p9b::BasicLexer lex;
    lex.loadProg(src.c_str());

    if (lex.hasError()) {
        std::cerr << std::format("Error in {}: {}\n", label, lex.errorMessage());
        return 1;
    }

    if (show_tokens) {
        std::cout << std::format("\nToken stream for: {}\n\n", label);
        const int count = dump_token_stream(lex, src.c_str());
        std::cout << std::format("\n{} token(s) stored  |  {} raw scanned\n",
            count, lex.totalTokens());
    } else {
        // Navigate to count stored tokens (excluding sentinel and null terminator).
        lex.gotoToken(1);
        int count = 0;
        while (lex.currTok() != p9b::BasToken::Null) {
            ++count;
            lex.advance();
        }
        std::cout << std::format("{}: OK  ({} tokens)\n", label, count);
    }
    return 0;
}

// =============================================================================
// Compile and run a source string; returns true on success.
// =============================================================================
static bool run_program(const std::string& src,
                        const std::string& label,
                        p9b::FunctionsDictionary& dict)
{
    p9b::BasicInterpreter interp(dict);
    const bool ok = interp.run(src, std::cin, std::cout);
    if (!ok) {
        std::cerr << std::format("[{}] {} error: {}\n",
            label, interp.phase(), interp.errorMessage());
    }
    return ok;
}

// =============================================================================
// Built-in function list
// =============================================================================
static void print_funcs(const p9b::FunctionsDictionary& dict) {
    // Collect and sort signatures.
    std::vector<std::string> sigs;
    sigs.reserve(dict.size());
    for (const auto& [key, _] : dict)
        sigs.push_back(key);
    std::sort(sigs.begin(), sigs.end());

    std::cout << std::format("\nRegistered built-in functions ({})\n", sigs.size());
    std::cout << std::string(56, '-') << '\n';
    std::cout << std::format("{:<20}  {}\n", "Signature", "Param types (N=number, S=string)");
    std::cout << std::string(56, '-') << '\n';

    for (const auto& sig : sigs) {
        // Split at '@' to show name and params separately.
        const auto at = sig.find('@');
        if (at != std::string::npos) {
            const std::string name   = sig.substr(0, at);
            const std::string params = sig.substr(at + 1);
            std::cout << std::format("{:<20}  ({})\n", name,
                params.empty() ? "no args" : params);
        } else {
            std::cout << std::format("{}\n", sig);
        }
    }
    std::cout << std::string(56, '-') << '\n';
}

// =============================================================================
// Help text
// =============================================================================
static void print_help() {
    std::cout <<
        "Plan9Basic interpreter  v0.8\n"
        "\n"
        "Usage:\n"
        "  p9b                      Interactive REPL\n"
        "  p9b <file.bas>           Compile and run a BASIC file\n"
        "  p9b --tokens <file.bas>  Tokenize file and dump token stream\n"
        "  p9b --funcs              List all registered built-in functions\n"
        "  p9b --help               Show this help\n"
        "\n"
        "REPL workflow:\n"
        "  Type BASIC lines — they accumulate in a buffer (prompt: '...')\n"
        "  Press Enter on an empty line to compile and run the buffer\n"
        "  All lines share the same execution context within one run\n"
        "\n"
        "REPL commands (prefix with '.'):\n"
        "  .help    Show this help\n"
        "  .funcs   List built-in functions\n"
        "  .run     Compile and execute the current buffer\n"
        "  .check   Tokenize the buffer and show token stream\n"
        "  .show    Print the current program buffer\n"
        "  .clear   Clear the program buffer\n"
        "  .tokens  Toggle per-line token display\n"
        "  .quit    Exit the REPL\n";
}

// =============================================================================
// REPL
// =============================================================================
static void run_repl(p9b::FunctionsDictionary& dict) {
    // ── Python-style startup banner ───────────────────────────────────────────
#if defined(_MSC_FULL_VER)
    constexpr int msc_major = _MSC_FULL_VER / 10'000'000;
    constexpr int msc_minor = (_MSC_FULL_VER / 100'000) % 100;
    constexpr int msc_patch = _MSC_FULL_VER % 100'000;
    const std::string compiler = std::format("MSVC {}.{}.{}", msc_major, msc_minor, msc_patch);
#else
    const std::string compiler = "GCC/Clang";
#endif

#if defined(_WIN32)
    constexpr std::string_view platform = "win32";
#else
    constexpr std::string_view platform = "linux";
#endif

    std::cout << std::format(
        "Plan9Basic 0.8 ({}, {}) [{}, 64-bit] on {}\n"
        "Type \".help\" for help, \".quit\" to exit.\n\n",
        __DATE__, __TIME__, compiler, platform);

    std::string        buffer;      // accumulated program text
    bool               show_tokens = false;
    std::string        line;

    for (;;) {
        std::cout << (buffer.empty() ? ">>> " : "... ");
        std::cout.flush();

        if (!std::getline(std::cin, line)) break; // EOF (Ctrl+Z / Ctrl+D)

        // ── Dot-commands ─────────────────────────────────────────────────────
        if (!line.empty() && line[0] == '.') {
            const auto        sp  = line.find(' ');
            const std::string cmd = p9b::utils::upper(
                line.substr(1, sp == std::string::npos ? std::string::npos : sp - 1));

            if (cmd == "QUIT" || cmd == "EXIT") {
                std::cout << "Bye.\n";
                break;
            }
            if (cmd == "HELP") {
                print_help();
                continue;
            }
            if (cmd == "FUNCS") {
                print_funcs(dict);
                continue;
            }
            if (cmd == "CLEAR") {
                buffer.clear();
                std::cout << "Buffer cleared.\n";
                continue;
            }
            if (cmd == "SHOW") {
                if (buffer.empty())
                    std::cout << "(buffer is empty)\n";
                else
                    std::cout << buffer << '\n';
                continue;
            }
            if (cmd == "TOKENS") {
                show_tokens = !show_tokens;
                std::cout << std::format("Token display: {}\n",
                    show_tokens ? "on" : "off");
                continue;
            }
            if (cmd == "CHECK") {
                if (buffer.empty()) {
                    std::cout << "(buffer is empty — type some BASIC code first)\n";
                } else {
                    tokenize_and_report(buffer, "<buffer>", show_tokens);
                }
                continue;
            }
            if (cmd == "RUN") {
                if (buffer.empty()) {
                    std::cout << "(buffer is empty — type some BASIC code first)\n";
                } else {
                    const auto cwd = std::filesystem::current_path();
                    const std::string expanded = preprocess(buffer, cwd);
                    run_program(expanded, "<buffer>", dict);
                }
                continue;
            }
            std::cout << std::format("Unknown command '{}'. Type '.help'.\n", line);
            continue;
        }

        // ── Regular BASIC input ───────────────────────────────────────────────
        // Empty line with non-empty buffer → execute (Python-style submit)
        if (line.empty()) {
            if (!buffer.empty()) {
                const auto cwd = std::filesystem::current_path();
                const std::string expanded = preprocess(buffer, cwd);
                run_program(expanded, "<buffer>", dict);
                buffer.clear();
            }
            continue;
        }

        buffer += line;
        buffer += '\n';

        if (show_tokens) {
            // Tokenize just this line for immediate feedback.
            tokenize_and_report(line, "<line>", true);
        }
    }
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Enable UTF-8 console I/O on Windows.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // ── Build function registry ───────────────────────────────────────────────
    p9b::FunctionsDictionary dict;
    p9b::numlib::register_functions(dict);
    p9b::strlib::register_functions(dict);

    // ── Argument parsing ──────────────────────────────────────────────────────
    if (argc == 1) {
        // No arguments → interactive REPL.
        run_repl(dict);
        return 0;
    }

    const std::string_view arg1{ argv[1] };

    if (arg1 == "--help" || arg1 == "-h") {
        print_help();
        return 0;
    }

    if (arg1 == "--funcs") {
        print_funcs(dict);
        return 0;
    }

    if (arg1 == "--tokens") {
        if (argc < 3) {
            std::cerr << "Error: --tokens requires a file path.\n";
            return 1;
        }
        std::string src;
        if (!load_file(argv[2], src)) {
            std::cerr << std::format("Error: cannot open '{}'\n", argv[2]);
            return 1;
        }
        return tokenize_and_report(src, argv[2], true);
    }

    // Default: compile and run every file passed on the command line.
    int errors = 0;
    for (int i = 1; i < argc; ++i) {
        std::string src;
        if (!load_file(argv[i], src)) {
            std::cerr << std::format("Error: cannot open '{}'\n", argv[i]);
            ++errors;
            continue;
        }
        // Phase 7: preprocess INCLUDE directives
        const auto base_dir = std::filesystem::path(argv[i]).parent_path();
        src = preprocess(src, base_dir);
        if (!run_program(src, argv[i], dict))
            ++errors;
    }
    return (errors > 0) ? 1 : 0;
}
