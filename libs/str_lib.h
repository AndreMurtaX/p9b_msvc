// =============================================================================
// str_lib.h  –  Plan9Basic: string built-in functions  (header-only)
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
// License : MIT  –  same as the original Pascal source
//
// Each function matches the BindFunction signature:
//   AsmData fn(std::span<AsmData> args)
//
// Key format: FUNCNAME@PARAMTYPES  where N=number, S=string, P=pointer.
// Functions whose name ends with '$' return a string in AsmData::s.
// =============================================================================
#pragma once

#include "p9b_types.h"
#include "p9b_utils.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <format>
#include <span>
#include <string>
#include <string_view>

namespace p9b::strlib {

// ── Internal helpers ─────────────────────────────────────────────────────────
namespace impl {

    // Get string arg i as a view (empty view if out of range).
    [[nodiscard]] inline std::string_view S(std::span<AsmData> a, std::size_t i) noexcept {
        return (i < a.size()) ? std::string_view{ a[i].s } : std::string_view{};
    }

    // Get numeric arg i (0.0 if out of range).
    [[nodiscard]] inline double N(std::span<AsmData> a, std::size_t i) noexcept {
        return (i < a.size()) ? a[i].n : 0.0;
    }

    // Build a numeric result.
    [[nodiscard]] inline AsmData RN(double v) noexcept {
        AsmData r;
        r.n = v;
        return r;
    }

    // Build a string result.
    [[nodiscard]] inline AsmData RS(std::string v) {
        AsmData r;
        r.s = std::move(v);
        return r;
    }

} // namespace impl

// =============================================================================
// Functions
// =============================================================================

// LEN(s$) → character count
inline AsmData fn_len(std::span<AsmData> a) {
    return impl::RN(static_cast<double>(impl::S(a, 0).size()));
}

// LEFT$(s$, n) → first n characters
inline AsmData fn_left(std::span<AsmData> a) {
    const std::string_view src   = impl::S(a, 0);
    const std::size_t      count = static_cast<std::size_t>(std::max(0.0, impl::N(a, 1)));
    return impl::RS(std::string{ src.substr(0, std::min(count, src.size())) });
}

// RIGHT$(s$, n) → last n characters
inline AsmData fn_right(std::span<AsmData> a) {
    const std::string_view src   = impl::S(a, 0);
    const std::size_t      count = static_cast<std::size_t>(std::max(0.0, impl::N(a, 1)));
    const std::size_t      start = (count >= src.size()) ? 0u : src.size() - count;
    return impl::RS(std::string{ src.substr(start) });
}

// MID$(s$, start)        → substring from start to end (1-based)
// MID$(s$, start, count) → substring from start, count chars (1-based)
inline AsmData fn_mid(std::span<AsmData> a) {
    const std::string_view src = impl::S(a, 0);
    const int raw = static_cast<int>(impl::N(a, 1));
    // 1-based: clamp to [1, size+1]
    const std::size_t start = (raw < 1) ? 0u : static_cast<std::size_t>(raw - 1);
    if (start >= src.size()) return impl::RS({});
    if (a.size() >= 3) {
        const std::size_t count = static_cast<std::size_t>(std::max(0.0, impl::N(a, 2)));
        return impl::RS(std::string{ src.substr(start, count) });
    }
    return impl::RS(std::string{ src.substr(start) });
}

// INSTR(s$, find$) → 1-based position of find$ in s$, 0 if not found
inline AsmData fn_instr2(std::span<AsmData> a) {
    const std::string_view hay    = impl::S(a, 0);
    const std::string_view needle = impl::S(a, 1);
    if (needle.empty()) return impl::RN(1.0);
    const auto pos = hay.find(needle);
    return impl::RN(pos == std::string_view::npos ? 0.0
                                                  : static_cast<double>(pos + 1));
}

// INSTR(start, s$, find$) → search from 1-based start position
inline AsmData fn_instr3(std::span<AsmData> a) {
    const std::size_t      from   = static_cast<std::size_t>(std::max(1.0, impl::N(a, 0))) - 1;
    const std::string_view hay    = impl::S(a, 1);
    const std::string_view needle = impl::S(a, 2);
    if (needle.empty())
        return impl::RN(from < hay.size() ? static_cast<double>(from + 1) : 0.0);
    const auto pos = hay.find(needle, from);
    return impl::RN(pos == std::string_view::npos ? 0.0
                                                  : static_cast<double>(pos + 1));
}

// STR$(n) → numeric value as string (shortest representation)
inline AsmData fn_str(std::span<AsmData> a) {
    return impl::RS(utils::float_to_str(impl::N(a, 0)));
}

// VAL(s$) → parse a number from s$ (0 on failure)
inline AsmData fn_val(std::span<AsmData> a) {
    bool ok   = false;
    const double v = utils::str_to_double(impl::S(a, 0), ok);
    return impl::RN(ok ? v : 0.0);
}

// CHR$(n) → single character string for ASCII code n
inline AsmData fn_chr(std::span<AsmData> a) {
    const int code = static_cast<int>(impl::N(a, 0));
    return impl::RS(std::string(1, static_cast<char>(code & 0xFF)));
}

// ASC(s$) → ASCII code of first character (0 if empty)
inline AsmData fn_asc(std::span<AsmData> a) {
    const std::string_view src = impl::S(a, 0);
    return impl::RN(src.empty() ? 0.0
                                : static_cast<double>(static_cast<unsigned char>(src[0])));
}

// UPPER$(s$) / LOWER$(s$) – ASCII case conversion
inline AsmData fn_upper(std::span<AsmData> a) {
    return impl::RS(utils::upper(impl::S(a, 0)));
}
inline AsmData fn_lower(std::span<AsmData> a) {
    std::string r{ impl::S(a, 0) };
    for (char& c : r)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return impl::RS(std::move(r));
}

// ── Trim helpers ──────────────────────────────────────────────────────────────
namespace impl {
    inline std::string do_trim(std::string_view s, bool left, bool right) {
        std::size_t b = 0;
        std::size_t e = s.size();
        if (left)  while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
        if (right) while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
        return std::string{ s.substr(b, e - b) };
    }
}

// TRIM$(s$) / LTRIM$(s$) / RTRIM$(s$)
inline AsmData fn_trim (std::span<AsmData> a) { return impl::RS(impl::do_trim(impl::S(a,0), true,  true )); }
inline AsmData fn_ltrim(std::span<AsmData> a) { return impl::RS(impl::do_trim(impl::S(a,0), true,  false)); }
inline AsmData fn_rtrim(std::span<AsmData> a) { return impl::RS(impl::do_trim(impl::S(a,0), false, true )); }

// REPLACE$(s$, find$, rep$) → replace all occurrences of find$ with rep$
inline AsmData fn_replace(std::span<AsmData> a) {
    std::string             result{ impl::S(a, 0) };
    const std::string_view  find  = impl::S(a, 1);
    const std::string_view  rep   = impl::S(a, 2);
    if (find.empty()) return impl::RS(std::move(result));
    std::string::size_type pos = 0;
    while ((pos = result.find(find, pos)) != std::string::npos) {
        result.replace(pos, find.size(), rep);
        pos += rep.size();
    }
    return impl::RS(std::move(result));
}

// SPLIT$(s$, delim$, n) → nth token (0-based), empty string if out of range
inline AsmData fn_split(std::span<AsmData> a) {
    const std::string_view src   = impl::S(a, 0);
    const std::string_view delim = impl::S(a, 1);
    const int              idx   = static_cast<int>(impl::N(a, 2));
    if (delim.empty()) return (idx == 0) ? impl::RS(std::string{ src }) : impl::RS({});
    int        count = 0;
    std::size_t start = 0;
    for (;;) {
        const auto end = src.find(delim, start);
        if (count == idx) {
            const std::size_t stop = (end == std::string_view::npos) ? src.size() : end;
            return impl::RS(std::string{ src.substr(start, stop - start) });
        }
        if (end == std::string_view::npos) break;
        start = end + delim.size();
        ++count;
    }
    return impl::RS({});
}

// REPEAT$(s$, n) → s$ repeated n times
inline AsmData fn_repeat(std::span<AsmData> a) {
    const std::string_view src   = impl::S(a, 0);
    const int              times = static_cast<int>(std::max(0.0, impl::N(a, 1)));
    std::string            result;
    result.reserve(src.size() * static_cast<std::size_t>(times));
    for (int i = 0; i < times; ++i) result += src;
    return impl::RS(std::move(result));
}

// HEX$(n) → unsigned hexadecimal string (uppercase, no prefix)
inline AsmData fn_hex(std::span<AsmData> a) {
    const auto v = static_cast<unsigned long long>(std::abs(impl::N(a, 0)));
    return impl::RS(std::format("{:X}", v));
}

// BIN$(n) → unsigned binary string (no prefix)
inline AsmData fn_bin(std::span<AsmData> a) {
    const auto v = static_cast<unsigned long long>(std::abs(impl::N(a, 0)));
    return impl::RS(std::format("{:b}", v));
}

// LPAD$(s$, n) → left-pad s$ with spaces so it is at least n characters wide
inline AsmData fn_lpad(std::span<AsmData> a) {
    const std::string_view src = impl::S(a, 0);
    const int              w   = static_cast<int>(impl::N(a, 1));
    if (w <= 0 || static_cast<std::size_t>(w) <= src.size())
        return impl::RS(std::string{ src });
    return impl::RS(std::string(static_cast<std::size_t>(w) - src.size(), ' ')
                    + std::string{ src });
}

// RPAD$(s$, n) → right-pad s$ with spaces so it is at least n characters wide
inline AsmData fn_rpad(std::span<AsmData> a) {
    const std::string_view src = impl::S(a, 0);
    const int              w   = static_cast<int>(impl::N(a, 1));
    if (w <= 0 || static_cast<std::size_t>(w) <= src.size())
        return impl::RS(std::string{ src });
    std::string result{ src };
    result.resize(static_cast<std::size_t>(w), ' ');
    return impl::RS(std::move(result));
}

// =============================================================================
// DATE$() — returns current date as "MM-DD-YYYY"
// TIME$() — returns current time as "HH:MM:SS"
// =============================================================================
inline AsmData fn_date(std::span<AsmData>) {
    const auto now  = std::chrono::system_clock::now();
    const auto tt   = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d-%02d-%04d",
                  tm_buf.tm_mon + 1, tm_buf.tm_mday, tm_buf.tm_year + 1900);
    return impl::RS(buf);
}

inline AsmData fn_time(std::span<AsmData>) {
    const auto now  = std::chrono::system_clock::now();
    const auto tt   = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    char buf[12];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
    return impl::RS(buf);
}

// =============================================================================
// Phase 4 — Advanced string functions
// =============================================================================

// FORMAT$(n, fmt$) — formata número com sintaxe PRINT USING, retorna string
inline AsmData fn_format_num(std::span<AsmData> args) {
    const double nval = args[0].n;
    std::string  fmt  = args[1].s;

    std::size_t i = 0;
    while (i < fmt.size()) {
        char c = fmt[i];
        if (c=='#'||c=='+'||c=='-'||c=='$'||c=='*'||c=='!'||c=='&'||c=='\\') break;
        ++i;
    }
    if (i >= fmt.size()) return impl::RS(fmt);

    std::string spec;
    std::size_t j = i;
    while (j < fmt.size()) {
        char nc = fmt[j];
        if (nc=='#'||nc=='.'||nc==','||nc=='+'||nc=='-'||nc=='$'||nc=='*')
            { spec += nc; ++j; }
        else break;
    }

    bool leading_plus   = (!spec.empty() && spec.front() == '+');
    bool trailing_minus = (!spec.empty() && spec.back()  == '-');
    bool has_comma      = (spec.find(',') != std::string::npos);
    bool has_dollar     = (spec.find('$') != std::string::npos);
    bool has_dot        = (spec.find('.') != std::string::npos);

    int ndigs_before = 0, ndigs_after = 0;
    bool after_dot = false;
    for (char c : spec) {
        if (c == '.') after_dot = true;
        else if (c == '#') { if (after_dot) ++ndigs_after; else ++ndigs_before; }
    }

    const bool   negative = (nval < 0.0);
    const double absval   = std::abs(nval);
    char nbuf[64];
    if (has_dot) std::snprintf(nbuf, sizeof(nbuf), "%.*f", ndigs_after, absval);
    else         std::snprintf(nbuf, sizeof(nbuf), "%.0f", absval);
    std::string num_str = nbuf;

    if (has_comma) {
        auto dp = num_str.find('.');
        std::size_t int_end = (dp != std::string::npos) ? dp : num_str.size();
        std::string int_part = num_str.substr(0, int_end);
        std::string dec_part = (dp != std::string::npos) ? num_str.substr(dp) : "";
        std::string with_commas;
        for (std::size_t k = 0; k < int_part.size(); ++k) {
            if (k > 0 && (int_part.size() - k) % 3 == 0) with_commas += ',';
            with_commas += int_part[k];
        }
        num_str = with_commas + dec_part;
    }

    std::string sign_str;
    if (negative) sign_str = "-";
    else if (leading_plus) sign_str = "+";

    std::string prefix = has_dollar ? "$" : "";
    std::string result = prefix + sign_str + num_str;

    int field_w = ndigs_before + (has_dot ? ndigs_after + 1 : 0);
    if (has_comma && ndigs_before > 3) field_w += (ndigs_before - 1) / 3;
    if (leading_plus || trailing_minus) ++field_w;
    if (has_dollar) ++field_w;

    while (static_cast<int>(result.size()) < field_w) result = ' ' + result;
    if (static_cast<int>(result.size()) > field_w) result = '%' + result;

    if (trailing_minus && !leading_plus) {
        result += (negative ? '-' : ' ');
    }

    return impl::RS(fmt.substr(0, i) + result + fmt.substr(j));
}

// SPACE$(n) — retorna string de n espaços
inline AsmData fn_space(std::span<AsmData> args) {
    const int n = std::max(0, static_cast<int>(args[0].n));
    return impl::RS(std::string(static_cast<std::size_t>(n), ' '));
}

// STRING$(n, c$) — repete primeiro char de c$ por n vezes
inline AsmData fn_string_n(std::span<AsmData> args) {
    const int         n  = std::max(0, static_cast<int>(args[0].n));
    const std::string c  = args[1].s;
    const char        ch = c.empty() ? ' ' : c[0];
    return impl::RS(std::string(static_cast<std::size_t>(n), ch));
}

// =============================================================================
// register_functions
// Insert all StrLib functions into the engine's FunctionsDictionary.
// Call once before executing any program.
// =============================================================================
inline void register_functions(FunctionsDictionary& dict) {
    auto add = [&](const char* sig, BindFunction fn) {
        dict[sig] = LinkFunction{ .farCall = true, .entry = fn };
    };

    add("LEN@S",        fn_len);

    add("LEFT$@SN",     fn_left);
    add("RIGHT$@SN",    fn_right);
    add("MID$@SN",      fn_mid);
    add("MID$@SNN",     fn_mid);

    add("INSTR@SS",     fn_instr2);
    add("INSTR@NSS",    fn_instr3);

    add("STR$@N",       fn_str);
    add("VAL@S",        fn_val);

    add("CHR$@N",       fn_chr);
    add("ASC@S",        fn_asc);

    add("UPPER$@S",     fn_upper);
    add("LOWER$@S",     fn_lower);

    add("TRIM$@S",      fn_trim);
    add("LTRIM$@S",     fn_ltrim);
    add("RTRIM$@S",     fn_rtrim);

    add("REPLACE$@SSS", fn_replace);
    add("SPLIT$@SSN",   fn_split);
    add("REPEAT$@SN",   fn_repeat);

    add("HEX$@N",       fn_hex);
    add("BIN$@N",       fn_bin);
    add("LPAD$@SN",     fn_lpad);
    add("RPAD$@SN",     fn_rpad);

    add("DATE$@",       fn_date);
    add("TIME$@",       fn_time);

    add("FORMAT$@NS",   fn_format_num);
    add("SPACE$@N",     fn_space);
    add("STRING$@NS",   fn_string_n);
}

} // namespace p9b::strlib
