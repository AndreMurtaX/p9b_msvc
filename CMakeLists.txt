cmake_minimum_required(VERSION 3.28)

project(Plan9Basic
    VERSION     1.0.0
    DESCRIPTION "Plan9Basic interpreter – C++ port"
    LANGUAGES   CXX
)

# ── Compiler standard ────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)   # -std=c++23, not -std=gnu++23

# ── Windows: force UTF-8 source and execution charset ────────────────────────
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ── Sub-targets ──────────────────────────────────────────────────────────────
add_subdirectory(engine)
add_subdirectory(libs)    # NumLib, StrLib – header-only built-in functions
add_subdirectory(cli)     # p9b command-line interpreter

# Define p9b como startup project padrão no Visual Studio
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
             PROPERTY VS_STARTUP_PROJECT p9b)

# ── Install (optional) ───────────────────────────────────────────────────────
# install(TARGETS p9b_engine ARCHIVE DESTINATION lib)
