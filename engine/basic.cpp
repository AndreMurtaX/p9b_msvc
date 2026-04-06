// =============================================================================
// basic.cpp  –  Plan9Basic: top-level interpreter orchestrator
// =============================================================================

#include "basic.h"

namespace p9b {

BasicInterpreter::BasicInterpreter(FunctionsDictionary& funcs)
    : funcs_(funcs) {}

bool BasicInterpreter::run(const std::string& source,
                           std::istream& in,
                           std::ostream& out)
{
    error_  = false;
    errMsg_.clear();
    phase_.clear();

    // ── Phase 1: Lex ─────────────────────────────────────────────────────────
    BasicLexer lex;
    lex.loadProg(source.c_str());
    if (lex.hasError()) {
        set_error("lex", lex.errorMessage());
        return false;
    }

    // ── Phase 2: Compile ─────────────────────────────────────────────────────
    Parser parser(lex, funcs_);
    if (!parser.compile()) {
        set_error("parse", parser.errorMessage());
        return false;
    }

    // ── Phase 3: Execute ──────────────────────────────────────────────────────
    BasicExec exec;
    exec.load(std::move(parser).result(), funcs_);
    exec.run(in, out);
    if (exec.hasError()) {
        set_error("exec", exec.errorMessage());
        return false;
    }

    return true;
}

void BasicInterpreter::set_error(const std::string& ph, const std::string& msg) {
    error_  = true;
    phase_  = ph;
    errMsg_ = msg;
}

} // namespace p9b
