// =============================================================================
// exec.cpp  –  Plan9Basic: stack-machine executor
// =============================================================================

#include "exec.h"
#include "p9b_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <fstream>
#ifdef _WIN32
#  include <conio.h>
#else
#  include <termios.h>
#  include <unistd.h>
#endif

namespace p9b {

// =============================================================================
// load / run
// =============================================================================
void BasicExec::load(Program&& prog, FunctionsDictionary& funcs) {
    prog_  = std::move(prog);
    funcs_ = &funcs;
    gnum_.assign(512, 0.0);
    gstr_.assign(512, {});
    stack_.reserve(256);
    kinds_.reserve(256);
    aux_.reserve(64);
    frames_.reserve(32);
    ip_     = 0;
    error_  = false;
    errMsg_.clear();
    data_ptr_ = 0;
    gnum_arr_.clear();
    gstr_arr_.clear();
    files_.clear();
    error_handler_ip_ = -1;
    err_resume_ip_    = 0;
    err_resume_next_  = 0;
    err_code_         = 0;
    err_line_         = 0;
    in_error_handler_ = false;
    start_time_       = std::chrono::steady_clock::now();
    trace_enabled_    = false;
}

void BasicExec::run(std::istream& in, std::ostream& out) {
    in_  = &in;
    out_ = &out;
    ip_  = 0;
    running_ = true;

    while (running_ && !error_ && ip_ < static_cast<int>(prog_.code.size()))
        step();
}

// =============================================================================
// Stack helpers
// =============================================================================
void BasicExec::push_num(double v) {
    AsmData d; d.n = v;
    stack_.push_back(d);
    kinds_.push_back(ExprKind::Number);
}

void BasicExec::push_str(std::string v) {
    AsmData d; d.s = std::move(v);
    stack_.push_back(std::move(d));
    kinds_.push_back(ExprKind::String);
}

double BasicExec::pop_num() {
    if (stack_.empty()) { set_error("Stack underflow"); return 0.0; }
    const double v = stack_.back().n;
    stack_.pop_back(); kinds_.pop_back();
    return v;
}

std::string BasicExec::pop_str() {
    if (stack_.empty()) { set_error("Stack underflow"); return {}; }
    std::string v = std::move(stack_.back().s);
    stack_.pop_back(); kinds_.pop_back();
    return v;
}

double BasicExec::peek_num() const {
    return stack_.empty() ? 0.0 : stack_.back().n;
}

// =============================================================================
// Variable access  (negative slot = local, non-negative = global)
// =============================================================================
double& BasicExec::num_ref(int slot) {
    if (slot < 0) {
        const int idx = -(slot + 1);
        auto& f = frames_.back().lnum;
        if (static_cast<int>(f.size()) <= idx) f.resize(static_cast<std::size_t>(idx + 1), 0.0);
        return f[static_cast<std::size_t>(idx)];
    }
    return gnum_[static_cast<std::size_t>(slot)];
}

std::string& BasicExec::str_ref(int slot) {
    if (slot < 0) {
        const int idx = -(slot + 1);
        auto& f = frames_.back().lstr;
        if (static_cast<int>(f.size()) <= idx) f.resize(static_cast<std::size_t>(idx + 1));
        return f[static_cast<std::size_t>(idx)];
    }
    return gstr_[static_cast<std::size_t>(slot)];
}

// =============================================================================
// Array element access helpers
// Arrays are 1-based; the slot indexes into the gnum_arr_ / gstr_arr_ maps.
// If the array has not been DIMmed yet a runtime error is raised.
// =============================================================================
double& BasicExec::arr_num_ref(int slot, int idx) {
    auto it = gnum_arr_.find(slot);
    if (it == gnum_arr_.end()) {
        set_error(std::format("Numeric array slot {} not allocated (missing DIM)", slot));
        static double dummy = 0.0;
        return dummy;
    }
    auto& v = it->second;
    if (idx < 1 || idx > static_cast<int>(v.size())) {
        set_error(std::format("Numeric array index {} out of bounds (1..{})", idx, v.size()));
        static double dummy = 0.0;
        return dummy;
    }
    return v[static_cast<std::size_t>(idx - 1)];
}

std::string& BasicExec::arr_str_ref(int slot, int idx) {
    auto it = gstr_arr_.find(slot);
    if (it == gstr_arr_.end()) {
        set_error(std::format("String array slot {} not allocated (missing DIM)", slot));
        static std::string dummy;
        return dummy;
    }
    auto& v = it->second;
    if (idx < 1 || idx > static_cast<int>(v.size())) {
        set_error(std::format("String array index {} out of bounds (1..{})", idx, v.size()));
        static std::string dummy;
        return dummy;
    }
    return v[static_cast<std::size_t>(idx - 1)];
}

// =============================================================================
// File helper
// =============================================================================
std::fstream* BasicExec::file_ref(int fn) {
    auto it = files_.find(fn);
    if (it == files_.end()) {
        set_error(std::format("File #{} is not open", fn));
        return nullptr;
    }
    return it->second.get();
}

// =============================================================================
// Error
// =============================================================================
void BasicExec::set_error(const std::string& msg, int code) {
    if (!error_) {
        error_ = true;
        const int err_ip = ip_ - 1;

        // Determine source location for the failing instruction
        int err_line_num = 0;
        std::string loc;
        if (err_ip >= 0 && err_ip < static_cast<int>(prog_.src_map.size())) {
            const auto [line, col] = prog_.src_map[static_cast<std::size_t>(err_ip)];
            err_line_num = line;
            if (line > 0)
                loc = " [line " + std::to_string(line) + ", col " + std::to_string(col) + "]";
        }

        // If an ON ERROR handler is active and we are not already inside it,
        // redirect to it instead of stopping.
        if (error_handler_ip_ >= 0 && !in_error_handler_) {
            err_code_         = code;
            err_line_         = err_line_num;
            err_resume_ip_    = err_ip;   // RESUME retries this instruction
            err_resume_next_  = ip_;      // RESUME NEXT skips to next instruction
            in_error_handler_ = true;
            error_   = false;   // clear error flag so execution continues
            running_ = true;
            ip_      = error_handler_ip_;
            return;
        }

        errMsg_ = msg + loc;
    }
    running_ = false;
}

// =============================================================================
// Main dispatch loop
// =============================================================================
void BasicExec::step() {
    const Instr& instr = prog_.code[static_cast<std::size_t>(ip_)];
    if (trace_enabled_) {
        int ln = 0;
        if (ip_ < static_cast<int>(prog_.src_map.size()))
            ln = prog_.src_map[static_cast<std::size_t>(ip_)].first;
        *out_ << std::format("[TRACE] ln={} op={}\n", ln,
                             static_cast<int>(instr.token));
    }
    ++ip_;

    switch (instr.token) {

    // ── Program control ───────────────────────────────────────────────────────
    case AsmToken::End:
        running_ = false;
        break;

    case AsmToken::Nop:
        break;

    // ── Push constants ────────────────────────────────────────────────────────
    case AsmToken::PushC:
        push_num(instr.n);
        break;

    case AsmToken::PushCS:
        push_str(prog_.strings[static_cast<std::size_t>(instr.i)]);
        break;

    // ── Push / pop numeric variables ─────────────────────────────────────────
    case AsmToken::Push:
        push_num(num_ref(instr.i));
        break;

    case AsmToken::PopStore:
        num_ref(instr.i) = pop_num();
        break;

    // ── Push / pop string variables ───────────────────────────────────────────
    case AsmToken::PushS:
        push_str(str_ref(instr.i));
        break;

    case AsmToken::PopStoreS:
        str_ref(instr.i) = pop_str();
        break;

    // ── Auxiliary stack (FOR loop limit/step) ─────────────────────────────────
    case AsmToken::PushAux:
        aux_.push_back(pop_num());
        break;

    case AsmToken::PopAux:
        if (!aux_.empty()) aux_.pop_back();
        break;

    case AsmToken::PushAuxTOS:      // peek top of aux → push to data stack
        if (aux_.empty()) { set_error("Aux stack underflow"); break; }
        push_num(aux_.back());
        break;

    // ── Arithmetic ────────────────────────────────────────────────────────────
    case AsmToken::Add: { const double b = pop_num(), a = pop_num(); push_num(a + b); break; }
    case AsmToken::Sub: { const double b = pop_num(), a = pop_num(); push_num(a - b); break; }
    case AsmToken::Mul: { const double b = pop_num(), a = pop_num(); push_num(a * b); break; }
    case AsmToken::Div: {
        const double b = pop_num(), a = pop_num();
        if (b == 0.0) { push_num(0.0); set_error("Division by zero", 11); break; }
        push_num(a / b);
        break;
    }
    case AsmToken::Mod: {
        const double b = pop_num(), a = pop_num();
        if (b == 0.0) { push_num(0.0); set_error("Division by zero", 11); break; }
        push_num(std::fmod(a, b));
        break;
    }
    case AsmToken::Pow: {
        const double b = pop_num(), a = pop_num();
        if (a < 0.0 && b != std::trunc(b)) {
            push_num(0.0);
            set_error("Illegal function call: negative base with fractional exponent", 5);
            break;
        }
        push_num(std::pow(a, b));
        break;
    }
    case AsmToken::Inv: push_num(-pop_num()); break;

    // ── String concatenation ──────────────────────────────────────────────────
    case AsmToken::AddS: {
        std::string b = pop_str(), a = pop_str();
        push_str(std::move(a) + std::move(b));
        break;
    }

    // ── Numeric comparisons (result: 1.0 = true, 0.0 = false) ─────────────────
    case AsmToken::Eq:  { const double b=pop_num(),a=pop_num(); push_num(a==b?1.0:0.0); break; }
    case AsmToken::Ne:  { const double b=pop_num(),a=pop_num(); push_num(a!=b?1.0:0.0); break; }
    case AsmToken::Lt:  { const double b=pop_num(),a=pop_num(); push_num(a< b?1.0:0.0); break; }
    case AsmToken::Le:  { const double b=pop_num(),a=pop_num(); push_num(a<=b?1.0:0.0); break; }
    case AsmToken::Gt:  { const double b=pop_num(),a=pop_num(); push_num(a> b?1.0:0.0); break; }
    case AsmToken::Ge:  { const double b=pop_num(),a=pop_num(); push_num(a>=b?1.0:0.0); break; }

    // ── String comparisons ────────────────────────────────────────────────────
    case AsmToken::EqS: { std::string b=pop_str(),a=pop_str(); push_num(a==b?1.0:0.0); break; }
    case AsmToken::NeS: { std::string b=pop_str(),a=pop_str(); push_num(a!=b?1.0:0.0); break; }
    case AsmToken::LtS: { std::string b=pop_str(),a=pop_str(); push_num(a< b?1.0:0.0); break; }
    case AsmToken::LeS: { std::string b=pop_str(),a=pop_str(); push_num(a<=b?1.0:0.0); break; }
    case AsmToken::GtS: { std::string b=pop_str(),a=pop_str(); push_num(a> b?1.0:0.0); break; }
    case AsmToken::GeS: { std::string b=pop_str(),a=pop_str(); push_num(a>=b?1.0:0.0); break; }

    // ── Logic ─────────────────────────────────────────────────────────────────
    case AsmToken::And: { const double b=pop_num(),a=pop_num(); push_num((a&&b)?1.0:0.0); break; }
    case AsmToken::Or:  { const double b=pop_num(),a=pop_num(); push_num((a||b)?1.0:0.0); break; }
    case AsmToken::Not: push_num(pop_num()==0.0?1.0:0.0); break;

    // ── Min / Max ─────────────────────────────────────────────────────────────
    case AsmToken::Max: { const double b=pop_num(),a=pop_num(); push_num(std::max(a,b)); break; }
    case AsmToken::Min: { const double b=pop_num(),a=pop_num(); push_num(std::min(a,b)); break; }

    // ── Control flow ──────────────────────────────────────────────────────────
    case AsmToken::Jump:
        ip_ = instr.i;
        break;

    case AsmToken::If:
        // Conditional jump: jump if top is zero (false)
        if (pop_num() == 0.0) ip_ = instr.i;
        break;

    // ── ON expr GOTO / GOSUB ─────────────────────────────────────────────────
    // instr.i = N (number of jump-table entries that follow immediately).
    // The N instructions after OnGoto/OnGosub are each a Jump with the target.
    // ip_ currently points to the first Jump entry.
    case AsmToken::OnGoto: {
        const int n        = instr.i;
        const int selector = static_cast<int>(pop_num());
        if (selector >= 1 && selector <= n) {
            // Read the target address from the selector-th Jump instruction.
            ip_ = prog_.code[static_cast<std::size_t>(ip_ + selector - 1)].i;
        } else {
            ip_ += n;   // skip entire jump table; fall through
        }
        break;
    }

    case AsmToken::OnGosub: {
        const int n        = instr.i;
        const int selector = static_cast<int>(pop_num());
        if (selector >= 1 && selector <= n) {
            const int target = prog_.code[static_cast<std::size_t>(ip_ + selector - 1)].i;
            frames_.push_back({ ip_ + n, false, {}, {} });  // return after jump table
            ip_ = target;
        } else {
            ip_ += n;   // out of range — fall through
        }
        break;
    }

    // ── FOR cycle test ────────────────────────────────────────────────────────
    // instr.n = variable slot (stored as double); instr.i = past-loop jump target
    // aux[top]=step, aux[top-1]=limit
    case AsmToken::ForCycle: {
        if (aux_.size() < 2) { set_error("FOR stack error"); break; }
        const int    var_s = static_cast<int>(instr.n);
        const double step  = aux_.back();
        if (step == 0.0) { set_error("FOR: STEP value cannot be zero", 5); break; }
        const double limit = aux_[aux_.size() - 2];
        const double var   = num_ref(var_s);
        const bool   done  = (step > 0.0) ? (var > limit) : (var < limit);
        if (done) ip_ = instr.i;   // jump past loop
        break;
    }

    // ── I/O ───────────────────────────────────────────────────────────────────
    case AsmToken::Print: {
        if (stack_.empty()) break;
        if (instr.i == 1 || kinds_.back() == ExprKind::String) {
            *out_ << pop_str();
        } else {
            *out_ << utils::float_to_str(pop_num());
        }
        break;
    }

    case AsmToken::CRLF:
        *out_ << '\n';
        break;

    case AsmToken::Cls:
        // Platform-specific clear; emit ANSI escape on terminals
        *out_ << "\033[2J\033[H";
        break;

    case AsmToken::Input: {
        std::string line;
        std::getline(*in_, line);
        bool ok = false;
        num_ref(instr.i) = utils::str_to_double(line, ok);
        break;
    }

    case AsmToken::InputS: {
        std::string line;
        std::getline(*in_, line);
        str_ref(instr.i) = line;
        break;
    }

    // ── Function calls ────────────────────────────────────────────────────────
    // GOSUB / forward function call both use CallNear
    case AsmToken::CallNear: {
        // Check if this is a GOSUB (target is a label) by peeking the instruction
        const Instr& target_instr = prog_.code[static_cast<std::size_t>(instr.i)];
        const bool is_func = (target_instr.token == AsmToken::InitFunc);
        frames_.push_back({ ip_, is_func, {}, {} });
        ip_ = instr.i;
        break;
    }

    case AsmToken::InitFunc: {
        // instr.i = index in prog_.strings of the param type signature, e.g. "SN"
        // Each char: 'S' = string param, 'N' = numeric param (in declaration order).
        // Args were pushed left-to-right so the last arg is on top of the stack.
        const std::string& sig = prog_.strings[static_cast<std::size_t>(instr.i)];
        const int n = static_cast<int>(sig.size());
        Frame& f = frames_.back();

        // Collect args from the stack in reverse order
        std::vector<AsmData> args(static_cast<std::size_t>(n));
        for (int k = n - 1; k >= 0; --k) {
            args[static_cast<std::size_t>(k)] = stack_.back();
            stack_.pop_back();
            kinds_.pop_back();
        }

        // Distribute into lnum / lstr with separate running indices per type.
        // This mirrors the parser's slot_num/slot_str counters (lnnext_/lsnext_).
        int num_idx = 0, str_idx = 0;
        for (int k = 0; k < n; ++k) {
            const auto& a = args[static_cast<std::size_t>(k)];
            if (sig[static_cast<std::size_t>(k)] == 'S') {
                if (static_cast<int>(f.lstr.size()) <= str_idx)
                    f.lstr.resize(static_cast<std::size_t>(str_idx + 1));
                f.lstr[static_cast<std::size_t>(str_idx++)] = a.s;
            } else {
                if (static_cast<int>(f.lnum.size()) <= num_idx)
                    f.lnum.resize(static_cast<std::size_t>(num_idx + 1), 0.0);
                f.lnum[static_cast<std::size_t>(num_idx++)] = a.n;
            }
        }
        break;
    }

    case AsmToken::RetFunction: {
        if (frames_.empty()) { set_error("RETURN without function"); break; }
        // Save return value (top of stack)
        const bool has_str = !kinds_.empty() && kinds_.back() == ExprKind::String;
        AsmData ret_val;
        ExprKind ret_kind = ExprKind::Number;
        if (!stack_.empty()) {
            ret_val  = stack_.back();
            ret_kind = kinds_.back();
            stack_.pop_back(); kinds_.pop_back();
        }
        ip_ = frames_.back().ret_ip;
        frames_.pop_back();
        // Push return value back
        stack_.push_back(ret_val);
        kinds_.push_back(has_str ? ExprKind::String : ret_kind);
        break;
    }

    case AsmToken::Return: {
        // GOSUB return
        if (frames_.empty()) { set_error("RETURN without GOSUB"); break; }
        ip_ = frames_.back().ret_ip;
        frames_.pop_back();
        break;
    }

    // ── Built-in function calls ───────────────────────────────────────────────
    case AsmToken::CallFar:
    case AsmToken::CallFarS:
    case AsmToken::CallFarP: {
        const std::string& sig = prog_.strings[static_cast<std::size_t>(instr.i)];
        auto it = funcs_->find(sig);
        if (it == funcs_->end()) { set_error("Unknown built-in: " + sig); break; }

        // Determine argument count from signature (chars after '@')
        const auto at  = sig.find('@');
        const int argc = (at == std::string::npos)
                       ? 0 : static_cast<int>(sig.size() - at - 1);

        std::vector<AsmData> args(static_cast<std::size_t>(argc));
        for (int k = argc - 1; k >= 0; --k) {
            args[static_cast<std::size_t>(k)] = stack_.back();
            stack_.pop_back(); kinds_.pop_back();
        }

        AsmData result = it->second.entry(std::span<AsmData>{ args });

        if (instr.token == AsmToken::CallFarS) {
            push_str(result.s);
        } else {
            push_num(result.n);
        }
        break;
    }

    // ── DATA / READ / RESTORE ─────────────────────────────────────────────────
    case AsmToken::Data:
    case AsmToken::DataS:
        // Data items are pre-scanned; this instruction is a no-op at runtime
        break;

    case AsmToken::Read: {
        if (data_ptr_ >= static_cast<int>(prog_.data.size())) {
            set_error("Out of DATA"); break;
        }
        const DataItem& di = prog_.data[static_cast<std::size_t>(data_ptr_++)];
        if (di.dataType == 'n') {
            num_ref(instr.i) = prog_.code[static_cast<std::size_t>(di.dataPos)].n;
        } else {
            set_error("Type mismatch in READ: expected number, got string", 13);
        }
        break;
    }

    case AsmToken::ReadS: {
        if (data_ptr_ >= static_cast<int>(prog_.data.size())) {
            set_error("Out of DATA"); break;
        }
        const DataItem& di = prog_.data[static_cast<std::size_t>(data_ptr_++)];
        if (di.dataType == '$') {
            str_ref(instr.i) = prog_.strings[
                static_cast<std::size_t>(prog_.code[static_cast<std::size_t>(di.dataPos)].i)];
        } else {
            set_error("Type mismatch in READ: expected string, got number", 13);
        }
        break;
    }

    case AsmToken::Restore:
        data_ptr_ = 0;
        break;

    // ── Array allocation  (DIM) ───────────────────────────────────────────────
    // instr.i = array slot;  stack top = size (number of elements)
    case AsmToken::DimArr: {
        const int size = static_cast<int>(pop_num());
        if (size < 1) { set_error("DIM size must be >= 1", 9); break; }
        if (size > 10'000'000) { set_error("DIM size exceeds maximum (10 000 000)", 9); break; }
        gnum_arr_[instr.i].assign(static_cast<std::size_t>(size), 0.0);
        break;
    }
    case AsmToken::DimArrS: {
        const int size = static_cast<int>(pop_num());
        if (size < 1) { set_error("DIM size must be >= 1", 9); break; }
        if (size > 10'000'000) { set_error("DIM size exceeds maximum (10 000 000)", 9); break; }
        gstr_arr_[instr.i].assign(static_cast<std::size_t>(size), std::string{});
        break;
    }

    // ── Array element read  (PushArr / PushArrS) ─────────────────────────────
    // instr.i = array slot;  stack top = 1-based index
    case AsmToken::PushArr: {
        const int idx = static_cast<int>(pop_num());
        push_num(arr_num_ref(instr.i, idx));
        break;
    }
    case AsmToken::PushArrS: {
        const int idx = static_cast<int>(pop_num());
        push_str(arr_str_ref(instr.i, idx));
        break;
    }

    // ── Array element write  (PopStoreArr / PopStoreArrS) ────────────────────
    // instr.i = array slot;  stack = [ index | value(top) ]
    case AsmToken::PopStoreArr: {
        const double val = pop_num();
        const int    idx = static_cast<int>(pop_num());
        arr_num_ref(instr.i, idx) = val;
        break;
    }
    case AsmToken::PopStoreArrS: {
        std::string  val = pop_str();
        const int    idx = static_cast<int>(pop_num());
        arr_str_ref(instr.i, idx) = std::move(val);
        break;
    }

    // ── Phase 3: Misc I/O ─────────────────────────────────────────────────────
    case AsmToken::InkeyS: {
        // Non-blocking key read; returns "" if no key waiting.
        // On Windows consoles: uses _kbhit()/_getch(); on non-console in_ → "".
#ifdef _WIN32
        if (_kbhit()) {
            const int ch = _getch();
            push_str(std::string(1, static_cast<char>(ch)));
        } else {
            push_str(std::string{});
        }
#else
        {
            struct termios oldt{}, newt{};
            // Only attempt raw-mode read when stdin is actually a terminal.
            if (tcgetattr(STDIN_FILENO, &oldt) != 0) {
                // stdin is not a TTY (e.g. redirected from file) — nothing ready.
                push_str(std::string{});
                break;
            }
            newt = oldt;
            newt.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
            newt.c_cc[VMIN]  = 0;
            newt.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            char ch = 0;
            const ssize_t nr = ::read(STDIN_FILENO, &ch, 1);
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            push_str(nr == 1 ? std::string(1, ch) : std::string{});
        }
#endif
        break;
    }

    case AsmToken::InputN: {
        // INPUT$(n) — read exactly n characters from stdin
        const int n = static_cast<int>(pop_num());
        if (n < 0) { set_error("INPUT$: argument must be >= 0"); break; }
        if (n == 0) { push_str(""); break; }   // gcount() would be stale; skip read
        std::string result;
        result.resize(static_cast<std::size_t>(n));
        in_->read(result.data(), n);
        result.resize(static_cast<std::size_t>(in_->gcount()));   // may be < n at EOF
        push_str(std::move(result));
        break;
    }

    case AsmToken::ShellCmd: {
        // SHELL command$ — execute OS command; execution continues after
        std::string cmd = pop_str();
        std::system(cmd.c_str()); // NOLINT(cert-env33-c)
        break;
    }

    // ── Phase 3: WRITE #n ─────────────────────────────────────────────────────
    case AsmToken::WriteFile: {
        const int fn = static_cast<int>(instr.n);
        auto* fs = file_ref(fn);
        if (!fs) break;
        if (instr.i == 1) {
            // String — enclose in double quotes, escape embedded quotes
            std::string s = pop_str();
            std::string out;
            out.reserve(s.size() + 2);
            out += '"';
            for (char c : s) {
                if (c == '"') out += '"'; // double-up quotes (CSV convention)
                out += c;
            }
            out += '"';
            *fs << out;
        } else {
            *fs << utils::float_to_str(pop_num());
        }
        break;
    }

    case AsmToken::WriteFileSep: {
        const int fn = static_cast<int>(instr.n);
        auto* fs = file_ref(fn);
        if (!fs) break;
        *fs << ',';
        break;
    }

    case AsmToken::WriteFileCRLF: {
        const int fn = static_cast<int>(instr.n);
        auto* fs = file_ref(fn);
        if (!fs) break;
        *fs << '\n';
        break;
    }

    // ── Phase 3: ON ERROR / RESUME / ERR / ERL ────────────────────────────────
    case AsmToken::OnError:
        // i == 0 → disable handler; i > 0 → set handler IP
        error_handler_ip_ = (instr.i == 0) ? -1 : instr.i;
        break;

    case AsmToken::ResumeStmt:
        // i == 0 → retry (jump to saved err_resume_ip_)
        // i == 1 → next statement (jump to err_resume_next_)
        // i == 2 → specific target (IP in instr.n)
        if (!in_error_handler_) {
            set_error("RESUME without active error handler", 20);
            break;
        }
        in_error_handler_ = false;   // allow future errors to invoke the handler again
        if (instr.i == 0) {
            ip_ = err_resume_ip_;
        } else if (instr.i == 1) {
            ip_ = err_resume_next_;
        } else {
            ip_ = static_cast<int>(instr.n);
        }
        break;

    case AsmToken::PushErr:
        push_num(static_cast<double>(err_code_));
        break;

    case AsmToken::PushErl:
        push_num(static_cast<double>(err_line_));
        break;

    // ── Phase 3: PRINT USING ──────────────────────────────────────────────────
    case AsmToken::PrintUsing: {
        // Stack (bottom→top): ... | format_string | value
        // Pops value, pops format_string, outputs formatted value,
        // pushes remaining format_string back.
        const bool is_str = (instr.i == 1);

        // Pop value first (top of stack)
        std::string sval;
        double      nval = 0.0;
        if (is_str) sval = pop_str();
        else        nval = pop_num();

        // Pop format string
        std::string fmt = pop_str();

        // Find next format spec in fmt, emit literal prefix + formatted value
        std::size_t i = 0;

        // Emit literal prefix (chars before first format spec)
        while (i < fmt.size()) {
            const char c = fmt[i];
            if (c == '#' || c == '+' || c == '-' || c == '$' || c == '*' ||
                c == '!' || c == '&' || c == '\\')
                break;
            *out_ << c;
            ++i;
        }

        if (i < fmt.size()) {
            const char spec_start = fmt[i];

            if (spec_start == '!' || spec_start == '&' || spec_start == '\\') {
                // ── String spec ──────────────────────────────────────────
                if (spec_start == '!') {
                    *out_ << (sval.empty() ? ' ' : sval[0]);
                    ++i;
                } else if (spec_start == '&') {
                    *out_ << sval;
                    ++i;
                } else {
                    // \...\  — fixed-width field (N+2 chars wide where N = spaces between)
                    std::size_t j = i + 1;
                    while (j < fmt.size() && fmt[j] != '\\') ++j;
                    const int width = static_cast<int>(j - i + 1); // +1 for closing '\'
                    // Print string left-justified in field, padded with spaces
                    if (static_cast<int>(sval.size()) >= width)
                        *out_ << sval.substr(0, static_cast<std::size_t>(width));
                    else {
                        *out_ << sval;
                        for (int k = static_cast<int>(sval.size()); k < width; ++k)
                            *out_ << ' ';
                    }
                    i = j + 1;
                }
            } else {
                // ── Numeric spec ─────────────────────────────────────────
                // Collect spec chars: # . , + - $ *
                std::string spec;
                while (i < fmt.size()) {
                    const char nc = fmt[i];
                    if (nc == '#' || nc == '.' || nc == ',' || nc == '+' ||
                        nc == '-' || nc == '$' || nc == '*')
                    { spec += nc; ++i; }
                    else break;
                }

                // Parse spec structure
                bool leading_plus  = (!spec.empty() && spec.front() == '+');
                bool trailing_minus= (!spec.empty() && spec.back()  == '-');
                bool has_comma     = (spec.find(',') != std::string::npos);
                bool has_dollar    = (spec.find('$') != std::string::npos);
                const auto dot_pos = spec.find('.');
                const bool has_dot = (dot_pos != std::string::npos);

                // Count digit positions before and after decimal
                int ndigs_before = 0, ndigs_after = 0;
                bool after_dot = false;
                for (char c : spec) {
                    if (c == '.')  { after_dot = true; }
                    else if (c == '#') {
                        if (after_dot) ++ndigs_after;
                        else           ++ndigs_before;
                    }
                }

                // Format the number
                const bool negative = (nval < 0.0);
                const double absval = std::abs(nval);
                char nbuf[64];
                if (has_dot)
                    std::snprintf(nbuf, sizeof(nbuf), "%.*f", ndigs_after, absval);
                else
                    std::snprintf(nbuf, sizeof(nbuf), "%.0f", absval);
                std::string num_str = nbuf;

                // Insert commas into integer part
                if (has_comma) {
                    const auto dp = num_str.find('.');
                    const std::size_t int_end = (dp != std::string::npos) ? dp : num_str.size();
                    std::string int_part  = num_str.substr(0, int_end);
                    const std::string dec_part  = (dp != std::string::npos) ? num_str.substr(dp) : "";
                    std::string with_commas;
                    for (std::size_t k = 0; k < int_part.size(); ++k) {
                        if (k > 0 && (int_part.size() - k) % 3 == 0)
                            with_commas += ',';
                        with_commas += int_part[k];
                    }
                    num_str = with_commas + dec_part;
                }

                // Sign
                std::string sign_str;
                if (negative) sign_str = "-";
                else if (leading_plus) sign_str = "+";

                // Dollar prefix
                std::string prefix = has_dollar ? "$" : "";

                std::string result = prefix + sign_str + num_str;

                // Compute total field width (# count + comma positions + sign + dot)
                int field_w = ndigs_before + (has_dot ? ndigs_after + 1 : 0);
                if (has_comma && ndigs_before > 3) field_w += (ndigs_before - 1) / 3;
                if (leading_plus || trailing_minus) ++field_w;
                if (has_dollar) ++field_w;

                // Pad to field width (right-justify)
                while (static_cast<int>(result.size()) < field_w)
                    result = ' ' + result;
                // Truncate if overflow (prefix with %)
                if (static_cast<int>(result.size()) > field_w)
                    result = '%' + result;

                // Trailing minus replaces space for negative
                if (trailing_minus && !leading_plus) {
                    if (negative) result += '-';
                    else          result += ' ';
                }

                *out_ << result;
            }
        }

        // Push remaining format string back
        push_str(fmt.substr(i));
        break;
    }

    case AsmToken::PrintUsingEnd: {
        // Pop remaining format string, emit any trailing literals, optional newline
        std::string fmt = pop_str();
        // Emit any trailing literal chars (after the last spec was consumed)
        for (char c : fmt) *out_ << c;
        if (instr.i == 1) *out_ << '\n';  // emit newline unless trailing semicolon
        break;
    }

    // ── File I/O ──────────────────────────────────────────────────────────────
    case AsmToken::OpenFile: {
        const int   mode  = instr.i;
        const int   fn    = static_cast<int>(instr.n);
        std::string fname = pop_str();
        if (files_.count(fn)) {
            set_error(std::format("File #{} is already open", fn));
            break;
        }
        auto fs = std::make_unique<std::fstream>();
        switch (mode) {
            case 0:  fs->open(fname, std::ios::in);                          break; // INPUT
            case 1:  fs->open(fname, std::ios::out | std::ios::trunc);       break; // OUTPUT
            default: fs->open(fname, std::ios::out | std::ios::app);         break; // APPEND
        }
        if (!fs->is_open()) {
            set_error(std::format("Cannot open file: {}", fname), 53);
            break;
        }
        files_[fn] = std::move(fs);
        break;
    }

    case AsmToken::CloseFile: {
        if (instr.i == 1) {
            // Close all
            files_.clear();
        } else {
            const int fn = static_cast<int>(instr.n);
            auto it = files_.find(fn);
            if (it != files_.end()) {
                it->second->close();
                files_.erase(it);
            }
        }
        break;
    }

    case AsmToken::PrintFile: {
        const int fn = static_cast<int>(instr.n);
        auto* fs = file_ref(fn);
        if (!fs) break;
        if (instr.i == 1) *fs << pop_str();
        else               *fs << utils::float_to_str(pop_num());
        break;
    }

    case AsmToken::PrintFileSep: {
        const int fn = static_cast<int>(instr.n);
        auto* fs = file_ref(fn);
        if (!fs) break;
        *fs << '\t';
        break;
    }

    case AsmToken::PrintFileCRLF: {
        const int fn = static_cast<int>(instr.n);
        auto* fs = file_ref(fn);
        if (!fs) break;
        *fs << '\n';
        break;
    }

    case AsmToken::InputFile: {
        const int fn = static_cast<int>(instr.n);
        auto* fs = file_ref(fn);
        if (!fs) break;
        std::string line;
        std::getline(*fs, line);
        bool ok = false;
        num_ref(instr.i) = utils::str_to_double(line, ok);
        break;
    }

    case AsmToken::InputFileS: {
        const int fn = static_cast<int>(instr.n);
        auto* fs = file_ref(fn);
        if (!fs) break;
        std::string line;
        std::getline(*fs, line);
        str_ref(instr.i) = std::move(line);
        break;
    }

    case AsmToken::LineInputFile: {
        const int fn = static_cast<int>(instr.n);
        auto* fs = file_ref(fn);
        if (!fs) break;
        std::string line;
        std::getline(*fs, line);
        str_ref(instr.i) = std::move(line);
        break;
    }

    case AsmToken::EofFile: {
        const int fn = static_cast<int>(pop_num());
        auto it = files_.find(fn);
        if (it == files_.end()) {
            set_error(std::format("File #{} is not open", fn));
            break;
        }
        // Use peek() to detect EOF accurately before the next read attempt
        push_num((it->second->peek() == std::char_traits<char>::eof()) ? -1.0 : 0.0);
        break;
    }

    // ── Debug / trace ─────────────────────────────────────────────────────────
    case AsmToken::Dump:
        *out_ << std::format("[DUMP ip={} stack={}]\n", ip_ - 1, stack_.size());
        break;

    case AsmToken::TraceOn:
        trace_enabled_ = true;
        break;

    case AsmToken::TraceOff:
        trace_enabled_ = false;
        break;

    case AsmToken::Trace:
        break;

    // ── Phase 6: PushTimer ───────────────────────────────────────────────────
    case AsmToken::PushTimer: {
        using namespace std::chrono;
        const double elapsed = duration_cast<duration<double>>(
            steady_clock::now() - start_time_).count();
        push_num(elapsed);
        break;
    }

    // ── Phase 6: LocateXY ────────────────────────────────────────────────────
    case AsmToken::LocateXY: {
        const int col = static_cast<int>(pop_num());
        const int row = static_cast<int>(pop_num());
        *out_ << std::format("\033[{};{}H", row, col);
        break;
    }

    // ── Phase 6: ColorSet ────────────────────────────────────────────────────
    case AsmToken::ColorSet: {
        const int bg = static_cast<int>(pop_num());
        const int fg = static_cast<int>(pop_num());
        if (bg < 0) {
            const int code = (fg >= 0 && fg < 8) ? 30 + fg : 90 + std::max(0, fg - 8);
            *out_ << std::format("\033[{}m", code);
        } else {
            const int fg_code = (fg >= 0 && fg < 8) ? 30 + fg : 90 + std::max(0, fg - 8);
            const int bg_code = 40 + (bg & 7);
            *out_ << std::format("\033[{};{}m", fg_code, bg_code);
        }
        break;
    }

    // ── Phase 8: Assert ──────────────────────────────────────────────────────
    case AsmToken::Assert: {
        std::string msg;
        if (instr.i == 1) msg = pop_str();
        const double cond = pop_num();
        if (cond == 0.0) {
            if (msg.empty()) {
                int ln = 0;
                if ((ip_-1) < static_cast<int>(prog_.src_map.size()))
                    ln = prog_.src_map[static_cast<std::size_t>(ip_-1)].first;
                msg = std::format("Assertion failed at line {}", ln);
            }
            set_error(msg, 5);
        }
        break;
    }

    // ── Unhandled ─────────────────────────────────────────────────────────────
    default:
        break;
    }
}

} // namespace p9b
