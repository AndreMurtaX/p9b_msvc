# =============================================================================
# cli/CMakeLists.txt  –  Plan9Basic command-line interpreter  (p9b)
# =============================================================================

add_executable(p9b main.cpp)

# Força o VS a lançar p9b.exe ao pressionar F5, independente do startup project.
set_target_properties(p9b PROPERTIES
    VS_DEBUGGER_COMMAND           "$<TARGET_FILE:p9b>"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:p9b>"
)

target_link_libraries(p9b
    PRIVATE
        p9b_engine    # lexer, types, utils  (provides engine/ include dir)
        p9b_numlib    # math built-ins       (provides libs/ include dir)
        p9b_strlib    # string built-ins     (provides libs/ include dir)
)

# Strict warnings – same policy as the engine library.
if(MSVC)
    target_compile_options(p9b PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(p9b PRIVATE -Wall -Wextra -Werror)
endif()
