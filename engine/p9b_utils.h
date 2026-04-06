// =============================================================================
// p9b_utils.h  –  Plan9Basic C++ port: utility functions  (header-only)
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
// License : MIT  –  same as the original Pascal source
//
// This header provides the small set of utility routines used by the core
// engine (lexer, parser, exec).  It is intentionally free of FMX, VCL and
// Delphi RTL dependencies; every function maps 1-to-1 to a TUtils method
// from the original UnitUtils.pas.
// =============================================================================
#pragma once

#include <string>
#include <string_view>
#include <charconv>   // std::from_chars  (C++17, full float support in MSVC 19.2+)
#include <algorithm>
#include <cctype>
#include <cassert>
#include <format>     // std::format  (C++20)

namespace p9b::utils {

// ─── string_code ─────────────────────────────────────────────────────────────
// Sum of the ordinal values of all characters in s.
// Mirrors TUtils::StringCode.
// Caller is responsible for passing an already-uppercased string when used
// to produce keyword hash-codes.
[[nodiscard]] inline int string_code(std::string_view s) noexcept {
    int result = 0;
    for (unsigned char c : s)
        result += static_cast<int>(c);
    return result;
}

// ─── str_to_double ───────────────────────────────────────────────────────────
// Parse a decimal / scientific-notation number from s.
// Sets ok = false if the entire string could not be converted.
// Uses std::from_chars: locale-independent, always treats '.' as decimal point.
// Mirrors TUtils::StrToFloat2.
[[nodiscard]] inline double str_to_double(std::string_view s, bool& ok) noexcept {
    if (s.empty()) { ok = false; return 0.0; }

    double value = 0.0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    ok = (ec == std::errc{}) && (ptr == s.data() + s.size());
    return value;
}

// ─── find_line_offset ────────────────────────────────────────────────────────
// Return the byte offset of the start of line n (0-based) inside a
// null-terminated string.  Returns 0 on out-of-range or null pointer.
// Mirrors TUtils::FindLine.
[[nodiscard]] inline int find_line_offset(const char* src, int n) noexcept {
    if (!src || n < 0) return 0;
    int line = 0, pos = 0;
    while (src[pos] != '\0' && line < n) {
        if (src[pos] == '\n') ++line;
        ++pos;
    }
    return pos;
}

// ─── str_line ────────────────────────────────────────────────────────────────
// Return the n-th line (0-based) from a null-terminated multiline string,
// stripping the line terminator.
// Mirrors TUtils::StrLine.
[[nodiscard]] inline std::string str_line(const char* src, int n) {
    if (!src || n < 0 || src[0] == '\0') return {};
    int pos          = find_line_offset(src, n);
    const char* start = src + pos;
    const char* end   = start;
    while (*end != '\0' && *end != '\n' && *end != '\r') ++end;
    return {start, end};
}

// ─── to_upper  /  upper ──────────────────────────────────────────────────────
// ASCII-only uppercase (BASIC identifiers and keywords are ASCII).
// to_upper modifies in-place; upper returns a new string.

inline void to_upper(std::string& s) noexcept {
    for (char& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

[[nodiscard]] inline std::string upper(std::string_view s) {
    std::string r{s};
    to_upper(r);
    return r;
}

// ─── limit_round ─────────────────────────────────────────────────────────────
// Round d to int and clamp to [mn, mx].
// Mirrors TUtils::LimitRound.
[[nodiscard]] inline int limit_round(double d, int mn, int mx) noexcept {
    int r = static_cast<int>(std::round(d));
    if (r < mn) r = mn;
    if (r > mx) r = mx;
    return r;
}

// ─── int_to_bool / bool_to_int ───────────────────────────────────────────────
[[nodiscard]] inline bool int_to_bool(int v) noexcept { return v != 0; }
[[nodiscard]] inline int  bool_to_int(bool v) noexcept { return v ? 1 : 0; }

// ─── float_to_str ────────────────────────────────────────────────────────────
// Convert a double to its shortest round-trip decimal string.
// Mirrors TUtils::FloatToStr2.
// std::format with {} selects the shortest representation (C++20).
[[nodiscard]] inline std::string float_to_str(double d) {
    // Use {:g} for general format: drops trailing zeros, uses exponent when
    // the value is very large or very small – matches classic BASIC output.
    return std::format("{:g}", d);
}

} // namespace p9b::utils
