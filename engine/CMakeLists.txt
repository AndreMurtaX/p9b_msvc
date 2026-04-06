add_library(p9b_engine STATIC
    lexer.cpp
    parser.cpp
    exec.cpp
    basic.cpp
)

target_include_directories(p9b_engine
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
)

# p9b_utils.h and p9b_types.h are header-only; no separate source needed.

target_compile_features(p9b_engine PUBLIC cxx_std_23)

# Strict warnings on both compilers
if(MSVC)
    target_compile_options(p9b_engine PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(p9b_engine PRIVATE -Wall -Wextra -Werror)
endif()
