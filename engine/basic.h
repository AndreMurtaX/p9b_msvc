// =============================================================================
// basic.h  –  Plan9Basic: top-level interpreter orchestrator
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
// License : MIT
//
// Ties together BasicLexer → Parser → BasicExec.
// Typical usage:
//
//   p9b::FunctionsDictionary dict;
//   p9b::numlib::register_functions(dict);
//   p9b::strlib::register_functions(dict);
//
//   p9b::BasicInterpreter interp(dict);
//   interp.run(source_code, std::cin, std::cout);
//   if (interp.hasError()) std::cerr << interp.errorMessage();
// =============================================================================
#pragma once

#include "p9b_types.h"
#include "lexer.h"
#include "parser.h"
#include "exec.h"

#include <iostream>
#include <string>

namespace p9b {

class BasicInterpreter {
public:
    explicit BasicInterpreter(FunctionsDictionary& funcs);

    // Compile and execute a source string.
    // I/O is directed to the supplied streams.
    // Returns true on success (no errors at any phase).
    bool run(const std::string& source,
             std::istream& in  = std::cin,
             std::ostream& out = std::cout);

    // Diagnostics
    [[nodiscard]] bool               hasError()     const noexcept { return error_; }
    [[nodiscard]] const std::string& errorMessage() const noexcept { return errMsg_; }
    [[nodiscard]] const std::string& phase()        const noexcept { return phase_; }

private:
    FunctionsDictionary& funcs_;
    bool        error_  = false;
    std::string errMsg_;
    std::string phase_;   // "lex", "parse", or "exec" when error_ is true

    void set_error(const std::string& ph, const std::string& msg);
};

} // namespace p9b
