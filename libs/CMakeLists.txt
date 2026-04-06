# =============================================================================
# libs/CMakeLists.txt  –  Plan9Basic built-in function libraries (header-only)
#
# Both libraries are INTERFACE targets: no .cpp to compile, no .lib produced.
# Any target that links them gets the include directory and C++23 feature flag.
# =============================================================================

# ── NumLib – math built-ins (ABS, SQR, SIN, COS, RND, …) ────────────────────
add_library(p9b_numlib INTERFACE)
target_include_directories(p9b_numlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(p9b_numlib    INTERFACE cxx_std_23)

# ── StrLib – string built-ins (LEN, MID$, INSTR, TRIM$, …) ──────────────────
add_library(p9b_strlib INTERFACE)
target_include_directories(p9b_strlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(p9b_strlib    INTERFACE cxx_std_23)
