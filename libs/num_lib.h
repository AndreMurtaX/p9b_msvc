// =============================================================================
// num_lib.h  –  Plan9Basic: math built-in functions  (header-only)
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
// License : MIT  –  same as the original Pascal source
//
// Each function matches the BindFunction signature:
//   AsmData fn(std::span<AsmData> args)
//
// The exec engine resolves calls by signature key, e.g. "ABS@N".
// Key format: FUNCNAME@PARAMTYPES  where N=number, S=string, P=pointer.
// Result type is implied by the function name (trailing $ → string return).
// =============================================================================
#pragma once

#include "p9b_types.h"

#include <cmath>
#include <numbers>
#include <random>
#include <algorithm>
#include <span>

namespace p9b::numlib {

// ── Internal helpers ─────────────────────────────────────────────────────────
namespace impl {

    // Return numeric value of arg i, or 0.0 if out of range.
    [[nodiscard]] inline double N(std::span<AsmData> a, std::size_t i) noexcept {
        return (i < a.size()) ? a[i].n : 0.0;
    }

    // Build a numeric result AsmData.
    [[nodiscard]] inline AsmData RN(double v) noexcept {
        AsmData r;
        r.n = v;
        return r;
    }

    // Mersenne-Twister RNG, seeded once per process.
    inline std::mt19937_64& rng() noexcept {
        static std::mt19937_64 engine{ std::random_device{}() };
        return engine;
    }

} // namespace impl

// =============================================================================
// Functions
// =============================================================================

// ABS(n) → |n|
inline AsmData fn_abs(std::span<AsmData> a) {
    return impl::RN(std::abs(impl::N(a, 0)));
}

// SQR(n) → √n
inline AsmData fn_sqr(std::span<AsmData> a) {
    return impl::RN(std::sqrt(impl::N(a, 0)));
}

// ── Trigonometry (radians) ────────────────────────────────────────────────────
inline AsmData fn_sin(std::span<AsmData> a) { return impl::RN(std::sin(impl::N(a, 0))); }
inline AsmData fn_cos(std::span<AsmData> a) { return impl::RN(std::cos(impl::N(a, 0))); }
inline AsmData fn_tan(std::span<AsmData> a) { return impl::RN(std::tan(impl::N(a, 0))); }
inline AsmData fn_atn(std::span<AsmData> a) { return impl::RN(std::atan(impl::N(a, 0))); }

// ── Logarithms / exponential ──────────────────────────────────────────────────
// LOG(n)   → natural logarithm (ln)
// LOG2(n)  → log base 2
// LOG10(n) → log base 10
// EXP(n)   → eⁿ
inline AsmData fn_log  (std::span<AsmData> a) { return impl::RN(std::log  (impl::N(a, 0))); }
inline AsmData fn_log2 (std::span<AsmData> a) { return impl::RN(std::log2 (impl::N(a, 0))); }
inline AsmData fn_log10(std::span<AsmData> a) { return impl::RN(std::log10(impl::N(a, 0))); }
inline AsmData fn_exp  (std::span<AsmData> a) { return impl::RN(std::exp  (impl::N(a, 0))); }

// ── Integer-part functions ────────────────────────────────────────────────────
// INT(n)   → floor toward −∞  (classic BASIC INT behaviour)
// FIX(n)   → truncate toward 0
// CEIL(n)  → ceiling
// FLOOR(n) → floor (same as INT, explicit alias)
inline AsmData fn_int  (std::span<AsmData> a) { return impl::RN(std::floor(impl::N(a, 0))); }
inline AsmData fn_fix  (std::span<AsmData> a) { return impl::RN(std::trunc(impl::N(a, 0))); }
inline AsmData fn_ceil (std::span<AsmData> a) { return impl::RN(std::ceil (impl::N(a, 0))); }
inline AsmData fn_floor(std::span<AsmData> a) { return impl::RN(std::floor(impl::N(a, 0))); }

// SGN(n) → −1 / 0 / +1
inline AsmData fn_sgn(std::span<AsmData> a) {
    const double v = impl::N(a, 0);
    return impl::RN(v < 0.0 ? -1.0 : v > 0.0 ? 1.0 : 0.0);
}

// ROUND(n)          → round to nearest integer
// ROUND(n, decimals)→ round to 'decimals' decimal places
inline AsmData fn_round(std::span<AsmData> a) {
    const double v = impl::N(a, 0);
    if (a.size() >= 2) {
        const double factor = std::pow(10.0, std::trunc(impl::N(a, 1)));
        return impl::RN(std::round(v * factor) / factor);
    }
    return impl::RN(std::round(v));
}

// RND()  → random double in [0, 1)
// RND(n) → random double in [0, n)
inline AsmData fn_rnd(std::span<AsmData> a) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    const double r = dist(impl::rng());
    if (a.empty()) return impl::RN(r);
    return impl::RN(r * impl::N(a, 0));
}

// PI() → π
inline AsmData fn_pi([[maybe_unused]] std::span<AsmData> a) {
    return impl::RN(std::numbers::pi);
}

// MAX(a, b) / MIN(a, b)
inline AsmData fn_max(std::span<AsmData> a) {
    return impl::RN(std::max(impl::N(a, 0), impl::N(a, 1)));
}
inline AsmData fn_min(std::span<AsmData> a) {
    return impl::RN(std::min(impl::N(a, 0), impl::N(a, 1)));
}

// POW(base, exp) → baseᵉˣᵖ
inline AsmData fn_pow(std::span<AsmData> a) {
    return impl::RN(std::pow(impl::N(a, 0), impl::N(a, 1)));
}

// =============================================================================
// Phase 5 — Bitwise operations
// =============================================================================

inline AsmData fn_band(std::span<AsmData> args) {
    return { static_cast<double>(static_cast<long long>(args[0].n) &
                                 static_cast<long long>(args[1].n)) };
}
inline AsmData fn_bor(std::span<AsmData> args) {
    return { static_cast<double>(static_cast<long long>(args[0].n) |
                                 static_cast<long long>(args[1].n)) };
}
inline AsmData fn_bxor(std::span<AsmData> args) {
    return { static_cast<double>(static_cast<long long>(args[0].n) ^
                                 static_cast<long long>(args[1].n)) };
}
inline AsmData fn_bnot(std::span<AsmData> args) {
    return { static_cast<double>(~static_cast<long long>(args[0].n)) };
}
inline AsmData fn_shl(std::span<AsmData> args) {
    const auto val   = static_cast<long long>(args[0].n);
    const int  shift = static_cast<int>(args[1].n);
    if (shift < 0 || shift >= 64) return { 0.0 };
    return { static_cast<double>(val << shift) };
}
inline AsmData fn_shr(std::span<AsmData> args) {
    // Logical (unsigned) right shift — matches GW-BASIC SHR behaviour.
    // Cast through unsigned so the shift fills with 0-bits regardless of sign.
    const auto val   = static_cast<unsigned long long>(static_cast<long long>(args[0].n));
    const int  shift = static_cast<int>(args[1].n);
    if (shift < 0 || shift >= 64) return impl::RN(0.0);
    return impl::RN(static_cast<double>(static_cast<long long>(val >> shift)));
}
inline AsmData fn_clng(std::span<AsmData> args) {
    return { static_cast<double>(static_cast<long long>(args[0].n)) };
}

// =============================================================================
// register_functions
// Insert all NumLib functions into the engine's FunctionsDictionary.
// Call once before executing any program.
// =============================================================================
inline void register_functions(FunctionsDictionary& dict) {
    auto add = [&](const char* sig, BindFunction fn) {
        dict[sig] = LinkFunction{ .farCall = true, .entry = fn };
    };

    add("ABS@N",    fn_abs);
    add("SQR@N",    fn_sqr);

    add("SIN@N",    fn_sin);
    add("COS@N",    fn_cos);
    add("TAN@N",    fn_tan);
    add("ATN@N",    fn_atn);

    add("LOG@N",    fn_log);
    add("LOG2@N",   fn_log2);
    add("LOG10@N",  fn_log10);
    add("EXP@N",    fn_exp);

    add("INT@N",    fn_int);
    add("FIX@N",    fn_fix);
    add("CEIL@N",   fn_ceil);
    add("FLOOR@N",  fn_floor);
    add("SGN@N",    fn_sgn);

    add("ROUND@N",  fn_round);
    add("ROUND@NN", fn_round);

    add("RND@",     fn_rnd);
    add("RND@N",    fn_rnd);

    add("PI@",      fn_pi);

    add("MAX@NN",   fn_max);
    add("MIN@NN",   fn_min);
    add("POW@NN",   fn_pow);

    add("BAND@NN",  fn_band);
    add("BOR@NN",   fn_bor);
    add("BXOR@NN",  fn_bxor);
    add("BNOT@N",   fn_bnot);
    add("SHL@NN",   fn_shl);
    add("SHR@NN",   fn_shr);
    add("CLNG@N",   fn_clng);
}

} // namespace p9b::numlib
