// =============================================================================
// exec.h  –  Plan9Basic: stack-machine executor
//
// Target : MSVC 19.4x  /std:c++latest  (Visual Studio 2026)
// Platform: Windows x64 / Linux x64
// License : MIT
//
// Executes the InstrArray produced by Parser.
// Uses two parallel data stacks:
//   stack_ / kinds_  — main data stack + ExprKind type tags
// Local variables are stored per call frame; globals in flat arrays.
// =============================================================================
#pragma once

#include "p9b_types.h"
#include "parser.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace p9b {

class BasicExec {
public:
    // Load a compiled program and set the function registry.
    void load(Program&& prog, FunctionsDictionary& funcs);

    // Execute from instruction 0.  I/O goes to in/out.
    void run(std::istream& in  = std::cin,
             std::ostream& out = std::cout);

    [[nodiscard]] bool               hasError()     const noexcept { return error_;  }
    [[nodiscard]] const std::string& errorMessage() const noexcept { return errMsg_; }

private:
    Program              prog_;
    FunctionsDictionary* funcs_ = nullptr;
    std::istream*        in_    = nullptr;
    std::ostream*        out_   = nullptr;

    // ── Data stack (value + type tag) ─────────────────────────────────────────
    std::vector<AsmData>   stack_;
    std::vector<ExprKind>  kinds_;

    // ── Auxiliary stack (FOR loop limit / step) ───────────────────────────────
    std::vector<double> aux_;

    // ── Variable storage ──────────────────────────────────────────────────────
    std::vector<double>      gnum_;   // global numerics  (slot ≥ 0)
    std::vector<std::string> gstr_;   // global strings   (slot ≥ 0)

    // ── Array storage (global only; slot → 1-based flat vector) ──────────────
    std::unordered_map<int, std::vector<double>>      gnum_arr_;
    std::unordered_map<int, std::vector<std::string>> gstr_arr_;

    // ── Call stack ────────────────────────────────────────────────────────────
    // Each frame holds the return address and the local variable arrays.
    struct Frame {
        int                      ret_ip;
        bool                     is_func;   // FUNCTION vs GOSUB
        std::vector<double>      lnum;
        std::vector<std::string> lstr;
    };
    std::vector<Frame> frames_;

    // ── File handle table ─────────────────────────────────────────────────────
    // Maps file number (1-based int) to an open fstream.
    // Mode 0 = INPUT (read), 1 = OUTPUT (write/trunc), 2 = APPEND (write/app).
    std::unordered_map<int, std::unique_ptr<std::fstream>> files_;

    // ── DATA / READ state ─────────────────────────────────────────────────────
    int data_ptr_ = 0;

    // ── ON ERROR state ────────────────────────────────────────────────────────
    int  error_handler_ip_  = -1;   // -1 = no handler; >= 0 = handler IP
    int  err_resume_ip_     =  0;   // IP to jump to on RESUME (retry)
    int  err_resume_next_   =  0;   // IP to jump to on RESUME NEXT (skip)
    int  err_code_          =  0;   // current error code (GW-BASIC style)
    int  err_line_          =  0;   // source line of last error
    bool in_error_handler_  = false; // guard against recursive handler invocation

    // ── Phase 6: Timer ────────────────────────────────────────────────────────
    std::chrono::steady_clock::time_point start_time_;

    // ── Phase 8: Trace ────────────────────────────────────────────────────────
    bool trace_enabled_ = false;

    // ── Execution state ───────────────────────────────────────────────────────
    int  ip_      = 0;
    bool running_ = false;
    bool error_   = false;
    std::string errMsg_;

    // ── Dispatch ──────────────────────────────────────────────────────────────
    void step();

    // ── Stack helpers ─────────────────────────────────────────────────────────
    void       push_num(double v);
    void       push_str(std::string v);
    double     pop_num();
    std::string pop_str();
    double     peek_num() const;

    // ── Variable access ───────────────────────────────────────────────────────
    // slot < 0  →  local -(slot+1)
    // slot >= 0 →  global slot
    double&      num_ref(int slot);
    std::string& str_ref(int slot);

    // ── Array element access (1-based index; grows array on DimArr) ───────────
    double&      arr_num_ref(int slot, int idx);
    std::string& arr_str_ref(int slot, int idx);

    // ── File helper ───────────────────────────────────────────────────────────
    // Returns the fstream* for the given file number, or nullptr + sets error.
    std::fstream* file_ref(int fn);

    // ── Error ─────────────────────────────────────────────────────────────────
    // code: GW-BASIC-style error number (default 5 = "Illegal function call")
    void set_error(const std::string& msg, int code = 5);
};

} // namespace p9b
