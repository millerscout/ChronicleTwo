#include "platform/crash.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <typeinfo>

#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#define CT_HAVE_CXXABI 1
#endif

#include "platform/breadcrumbs.hpp"
#include "platform/build_info.hpp"
#include "platform/check.hpp"
#include "platform/log.hpp"
#include "platform/paths.hpp"

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define CT_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(CT_ASAN)
#define CT_ASAN 1
#endif
#ifdef CT_ASAN
// Declared here: this clang does not ship sanitizer/common_interface_defs.h on the default include path.
extern "C" void __sanitizer_set_death_callback(void (*callback)(void));

// SDL and ALSA leak a small buffer at process exit. LeakSanitizer then makes --gtest_list_tests exit 1,
// and ctest refuses to discover the suite. ASAN_OPTIONS=detect_leaks=1 turns the check back on.
extern "C" const char *__asan_default_options() {
    return "detect_leaks=0";
}
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include <unwind.h>
#if defined(__linux__)
#include <link.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <pthread.h>
#endif
extern char **environ;
#endif

namespace {

constexpr int         kMaxFrames = 64;
constexpr int         kMaxModules = 160;
constexpr std::size_t kPathSize = 1024;

struct Module {
    std::uintptr_t base = 0;  // what the module's file addresses are relative to
    std::uintptr_t start = 0;
    std::uintptr_t end = 0;
    char           name[256] = {};
};

Module            modules[kMaxModules];
std::atomic<int>  module_count{0};
char              report_dir[kPathSize] = {};
char              session_log[kPathSize] = {};
char              command_line[kPathSize] = {};
char              symbolizer[kPathSize] = {};
char              executable[kPathSize] = {};
bool              symbolizer_llvm = false;
long              utc_offset = 0;
std::atomic<bool> installed{false};
std::atomic<bool> handling{false};
char              reason[1024] = {};
const auto        start_time = std::chrono::steady_clock::now();

// A fixed buffer in front of write(2): the crash path may not allocate or use stdio.
class Writer {
  public:
    explicit Writer(int fd) : fd_(fd) {}
    ~Writer() { Flush(); }
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;

    Writer &Str(std::string_view text) {
        for (char c : text) {
            Put(c);
        }
        return *this;
    }
    Writer &Str(const char *text) { return Str(std::string_view(text ? text : "")); }
    Writer &Dec(long long value) {
        char digits[24];
        int  count = 0;
        bool negative = value < 0;
        auto magnitude = negative ? 0ull - static_cast<unsigned long long>(value) : static_cast<unsigned long long>(value);
        do {
            digits[count++] = static_cast<char>('0' + magnitude % 10);
            magnitude /= 10;
        } while (magnitude != 0);
        if (negative) {
            Put('-');
        }
        while (count > 0) {
            Put(digits[--count]);
        }
        return *this;
    }
    // width 0: as few digits as the value needs.
    Writer &Hex(unsigned long long value, int width = 16) {
        Str("0x");
        if (width == 0) {
            width = 1;
            while (width < 16 && (value >> (width * 4)) != 0) {
                ++width;
            }
        }
        for (int shift = (width - 1) * 4; shift >= 0; shift -= 4) {
            Put("0123456789abcdef"[(value >> shift) & 0xf]);
        }
        return *this;
    }
    Writer &Pad(std::string_view text, std::size_t width) {
        Str(text);
        for (std::size_t i = text.size(); i < width; ++i) {
            Put(' ');
        }
        return *this;
    }
    void Flush() {
        const char *data = buffer_;
        while (size_ > 0 && fd_ >= 0) {
#ifdef _WIN32
            const int written = _write(fd_, data, static_cast<unsigned>(size_));
#else
            const ssize_t written = ::write(fd_, data, size_);
#endif
            if (written <= 0) {
                if (written < 0 && errno == EINTR) {
                    continue;
                }
                break;
            }
            data += written;
            size_ -= static_cast<std::size_t>(written);
        }
        size_ = 0;
    }

  private:
    void Put(char c) {
        if (size_ == sizeof buffer_) {
            Flush();
        }
        buffer_[size_++] = c;
    }

    int         fd_;
    char        buffer_[1024];
    std::size_t size_ = 0;
};

void CopyString(char *out, std::size_t size, std::string_view text) {
    const std::size_t count = std::min(text.size(), size - 1);
    std::memcpy(out, text.data(), count);
    out[count] = '\0';
}

void Append(char *out, std::size_t size, std::string_view text) {
    const std::size_t used = std::strlen(out);
    CopyString(out + used, size - used, text);
}

void AppendDec(char *out, std::size_t size, long long value, int width = 0) {
    char digits[24];
    int  count = 0;
    auto magnitude = static_cast<unsigned long long>(value < 0 ? -value : value);
    do {
        digits[count++] = static_cast<char>('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0 || count < width);
    char text[24];
    int  length = 0;
    if (value < 0) {
        text[length++] = '-';
    }
    while (count > 0) {
        text[length++] = digits[--count];
    }
    Append(out, size, std::string_view(text, static_cast<std::size_t>(length)));
}

void AppendHex(char *out, std::size_t size, unsigned long long value) {
    char text[20] = "0x";
    int  length = 2;
    bool started = false;
    for (int shift = 60; shift >= 0; shift -= 4) {
        const unsigned digit = (value >> shift) & 0xf;
        if (digit != 0 || started || shift == 0) {
            text[length++] = "0123456789abcdef"[digit];
            started = true;
        }
    }
    Append(out, size, std::string_view(text, static_cast<std::size_t>(length)));
}

// Days since 1970-01-01 to a civil date (Howard Hinnant's algorithm); localtime is not async-signal-safe.
void CivilFromDays(long long days, int &year, unsigned &month, unsigned &day) {
    days += 719468;
    const long long era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned  doe = static_cast<unsigned>(days - era * 146097);
    const unsigned  yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned  doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned  mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp < 10 ? mp + 3 : mp - 9;
    year = static_cast<int>(yoe + era * 400) + (month <= 2);
}

// "20261018-152711" in local time, from the offset taken at install.
void AppendStamp(char *out, std::size_t size, bool readable) {
    const long long local = static_cast<long long>(std::time(nullptr)) + utc_offset;
    const long long days = local >= 0 ? local / 86400 : (local - 86399) / 86400;
    const long long seconds = local - days * 86400;
    int             year = 0;
    unsigned        month = 0;
    unsigned        day = 0;
    CivilFromDays(days, year, month, day);
    AppendDec(out, size, year, 4);
    if (readable) {
        Append(out, size, "-");
    }
    AppendDec(out, size, month, 2);
    if (readable) {
        Append(out, size, "-");
    }
    AppendDec(out, size, day, 2);
    Append(out, size, readable ? " " : "-");
    AppendDec(out, size, seconds / 3600, 2);
    if (readable) {
        Append(out, size, ":");
    }
    AppendDec(out, size, seconds / 60 % 60, 2);
    if (readable) {
        Append(out, size, ":");
    }
    AppendDec(out, size, seconds % 60, 2);
}

long ProcessId() {
#ifdef _WIN32
    return _getpid();
#else
    return static_cast<long>(::getpid());
#endif
}

void ReportPath(char *path, std::size_t size, std::string_view extension) {
    CopyString(path, size, report_dir);
    Append(path, size, "/crash-");
    AppendStamp(path, size, false);
    Append(path, size, "-");
    AppendDec(path, size, ProcessId());
    Append(path, size, extension);
}

const Module *FindModule(std::uintptr_t address) {
    const int count = module_count.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) {
        if (address >= modules[i].start && address < modules[i].end) {
            return &modules[i];
        }
    }
    return nullptr;
}

std::string_view BaseName(const char *path) {
    const std::string_view name(path);
    const auto             slash = name.find_last_of("/\\");
    return slash == std::string_view::npos ? name : name.substr(slash + 1);
}

void WriteHeader(Writer &out, std::string_view what, std::string_view detail, std::uintptr_t address, bool have_address) {
    const BuildInfo &build = BuildInfoGet();
    char             stamp[32] = {};
    AppendStamp(stamp, sizeof stamp, true);
    const double uptime = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();

    out.Str("Dark Chronicle crash report\n\n");
    out.Pad("crash:", 14).Str(what).Str(detail).Str("\n");
    if (have_address) {
        out.Pad("address:", 14).Hex(address).Str(address == 0 ? " (or not reported: see the registers)\n" : "\n");
    }
    if (reason[0] != '\0') {
        out.Pad("reason:", 14).Str(reason).Str("\n");
    }
    out.Pad("time:", 14).Str(stamp).Str("\n");
    out.Pad("uptime:", 14).Dec(static_cast<long long>(uptime)).Str(" s\n");
    out.Pad("pid:", 14).Dec(ProcessId()).Str("\n");
#if defined(__linux__)
    char thread_name[17] = {};
    ::prctl(PR_GET_NAME, thread_name, 0, 0, 0);
    out.Pad("thread:", 14).Str(thread_name).Str(" (tid ").Dec(::syscall(SYS_gettid)).Str(")\n");
#elif defined(__APPLE__)
    std::uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    out.Pad("thread:", 14).Str(pthread_main_np() ? "main" : "other").Str(" (tid ").Dec(static_cast<long long>(tid)).Str(")\n");
#elif defined(_WIN32)
    out.Pad("thread:", 14).Str("tid ").Dec(static_cast<long long>(GetCurrentThreadId())).Str("\n");
#endif
    out.Pad("build id:", 14).Str(build.build_id[0] ? build.build_id : "(none)").Str("\n");
    out.Pad("load base:", 14).Hex(build.load_base).Str("\n");
    out.Pad("version:", 14).Str(build.version).Str("\n");
    out.Pad("platform:", 14).Str(build.platform).Str("\n");
    out.Pad("command line:", 14).Str(command_line).Str("\n");
    out.Pad("session log:", 14).Str(session_log[0] ? session_log : "(none)").Str("\n");
}

void WriteFrames(Writer &out, const std::uintptr_t *frames, int count) {
    out.Str("\n== backtrace ==\n");
    for (int i = 0; i < count; ++i) {
        out.Str(i < 10 ? " #" : "#").Dec(i).Str("  ").Hex(frames[i]).Str("  ");
        if (const Module *module = FindModule(frames[i])) {
            out.Str(module->name[0] ? BaseName(module->name) : BaseName(executable)).Str("+").Hex(frames[i] - module->base, 0);
        } else {
            out.Str("?");
        }
        out.Str("\n");
    }
}

void WriteTail(Writer &out, int fd) {
    out.Str("\n== breadcrumbs ==\n");
    out.Flush();
    BreadcrumbsWrite(fd);
    out.Str("\n== recent log ==\n");
    out.Flush();
    LogWriteRecent(fd);
    out.Str("\n== unflushed stderr ==\n");
    out.Flush();
    LogDrainCaptured(fd);
    out.Str("\n== end ==\n");
}

// "crash report: <path>" on the terminal and in the session file.
void Announce(const char *path) {
    for (const int fd : {LogTerminalFd(), LogFileFd()}) {
        if (fd < 0) {
            continue;
        }
        Writer out(fd);
        out.Str("crash report: ").Str(path).Str("\n");
    }
}

#ifndef _WIN32

struct sigaction previous[NSIG];
constexpr int    kFatalSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
alignas(16) unsigned char alternate_stack[64 * 1024];

std::string_view SignalName(int signal) {
    switch (signal) {
        case SIGSEGV:
            return "SIGSEGV";
        case SIGBUS:
            return "SIGBUS";
        case SIGILL:
            return "SIGILL";
        case SIGFPE:
            return "SIGFPE";
        case SIGABRT:
            return "SIGABRT";
    }
    return "signal";
}

std::string_view CodeName(int signal, int code) {
    if (code == SI_USER) {
        return "SI_USER";
    }
#ifdef SI_KERNEL
    if (code == SI_KERNEL) {
        return "SI_KERNEL (non-canonical address or protection fault)";
    }
#endif
#ifdef SI_TKILL
    if (code == SI_TKILL) {
        return "SI_TKILL";
    }
#endif
    if (signal == SIGSEGV) {
        if (code == SEGV_MAPERR) {
            return "SEGV_MAPERR (address not mapped)";
        }
        if (code == SEGV_ACCERR) {
            return "SEGV_ACCERR (access not permitted)";
        }
    }
    if (signal == SIGBUS && code == BUS_ADRALN) {
        return "BUS_ADRALN";
    }
    if (signal == SIGFPE && code == FPE_INTDIV) {
        return "FPE_INTDIV (integer divide by zero)";
    }
    return "";
}

std::uintptr_t FaultPc(const void *context) {
    if (!context) {
        return 0;
    }
    const auto *uc = static_cast<const ucontext_t *>(context);
#if defined(__linux__) && defined(__x86_64__)
    return static_cast<std::uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
#elif defined(__linux__) && defined(__aarch64__)
    return static_cast<std::uintptr_t>(uc->uc_mcontext.pc);
#elif defined(__APPLE__) && defined(__aarch64__)
    return static_cast<std::uintptr_t>(uc->uc_mcontext->__ss.__pc);
#else
    (void)uc;
    return 0;
#endif
}

void WriteRegisters(Writer &out, const void *context) {
    if (!context) {
        return;
    }
    out.Str("\n== registers ==\n");
    const auto *uc = static_cast<const ucontext_t *>(context);
#if defined(__linux__) && defined(__x86_64__)
    static constexpr std::pair<const char *, int> kRegisters[] = {
        {"rax", REG_RAX}, {"rbx", REG_RBX}, {"rcx", REG_RCX}, {"rdx", REG_RDX}, {"rsi", REG_RSI}, {"rdi", REG_RDI},
        {"rbp", REG_RBP}, {"rsp", REG_RSP}, {"r8", REG_R8},   {"r9", REG_R9},   {"r10", REG_R10}, {"r11", REG_R11},
        {"r12", REG_R12}, {"r13", REG_R13}, {"r14", REG_R14}, {"r15", REG_R15}, {"rip", REG_RIP}, {"efl", REG_EFL},
    };
    int column = 0;
    for (const auto &[name, index] : kRegisters) {
        out.Pad(name, 4).Hex(static_cast<unsigned long long>(uc->uc_mcontext.gregs[index])).Str(++column % 3 ? "   " : "\n");
    }
#elif defined(__linux__) && defined(__aarch64__)
    for (int i = 0; i < 31; ++i) {
        out.Str("x").Dec(i).Str(i < 10 ? "  " : " ").Hex(uc->uc_mcontext.regs[i]).Str((i + 1) % 3 ? "   " : "\n");
    }
    out.Str("\nsp  ").Hex(uc->uc_mcontext.sp).Str("   pc  ").Hex(uc->uc_mcontext.pc).Str("\n");
#elif defined(__APPLE__) && defined(__aarch64__)
    const auto &state = uc->uc_mcontext->__ss;
    for (int i = 0; i < 29; ++i) {
        out.Str("x").Dec(i).Str(i < 10 ? "  " : " ").Hex(state.__x[i]).Str((i + 1) % 3 ? "   " : "\n");
    }
    out.Str("\nfp  ").Hex(state.__fp).Str("   lr  ").Hex(state.__lr).Str("   sp  ").Hex(state.__sp).Str("   pc  ")
        .Hex(state.__pc).Str("\n");
#else
    (void)uc;
    out.Str("(not available on this platform)\n");
#endif
}

struct UnwindState {
    std::uintptr_t *frames;
    int             count;
};

_Unwind_Reason_Code UnwindFrame(_Unwind_Context *context, void *data) {
    auto *state = static_cast<UnwindState *>(data);
    int   before = 0;
    const auto ip = static_cast<std::uintptr_t>(_Unwind_GetIPInfo(context, &before));
    if (ip == 0) {
        return _URC_END_OF_STACK;
    }
    state->frames[state->count++] = ip;
    return state->count >= kMaxFrames ? _URC_END_OF_STACK : _URC_NO_REASON;
}

// The frames from the faulting instruction outwards; the handler's own frames are dropped.
int CollectFrames(std::uintptr_t fault_pc, std::uintptr_t *frames) {
    std::uintptr_t raw[kMaxFrames];
    UnwindState    state{raw, 0};
    _Unwind_Backtrace(UnwindFrame, &state);
    int first = -1;
    for (int i = 0; i < state.count; ++i) {
        if (raw[i] == fault_pc) {
            first = i;
            break;
        }
    }
    int count = 0;
    if (first < 0) {
        if (fault_pc != 0) {
            frames[count++] = fault_pc;
        }
        first = 0;
    }
    for (int i = first; i < state.count && count < kMaxFrames; ++i) {
        frames[count++] = raw[i];
    }
    return count;
}

// Runs the symbolizer on the frames inside the executable, with its output appended to the report. Uses vfork
// and execve alone, so a heap broken by the crash does not matter.
void Symbolize(int fd, const std::uintptr_t *frames, int count) {
    Writer out(fd);
    out.Str("\n== symbols ==\n");
    if (symbolizer[0] == '\0') {
        out.Str("symbolization skipped: neither llvm-symbolizer nor addr2line is on PATH\n");
        return;
    }
    const BuildInfo &build = BuildInfoGet();
    const Module    *main_module = module_count.load() > 0 ? &modules[0] : nullptr;

    static char  image[64];
    static char  addresses[kMaxFrames][24];
    static char *argv[kMaxFrames + 12];
    int          argc = 0;
#if defined(__linux__)
    CopyString(image, sizeof image, "/proc/");
    AppendDec(image, sizeof image, ProcessId());
    Append(image, sizeof image, "/exe");
#else
    CopyString(image, sizeof image, "");
#endif
    static char obj_arg[kPathSize + 8];
    static char load_arg[32];
    argv[argc++] = symbolizer;
#if defined(__APPLE__)
    // atos takes absolute addresses with the load address it should assume.
    static char atos_o[] = "-o";
    static char atos_l[] = "-l";
    argv[argc++] = atos_o;
    argv[argc++] = executable;
    load_arg[0] = '\0';
    AppendHex(load_arg, sizeof load_arg, build.load_base);
    argv[argc++] = atos_l;
    argv[argc++] = load_arg;
#else
    if (symbolizer_llvm) {
        static char demangle[] = "--demangle";
        static char inlines[] = "--inlines";
        static char print_address[] = "--print-address";
        CopyString(obj_arg, sizeof obj_arg, "--obj=");
        Append(obj_arg, sizeof obj_arg, image);
        argv[argc++] = obj_arg;
        argv[argc++] = demangle;
        argv[argc++] = inlines;
        argv[argc++] = print_address;
    } else {
        static char e[] = "-e";
        static char flags[] = "-fCipa";
        argv[argc++] = e;
        argv[argc++] = image;
        argv[argc++] = flags;
    }
#endif
    const auto in_main = [&](int i) {
        return main_module && frames[i] >= main_module->start && frames[i] < main_module->end;
    };
    // The outermost frame after libc's start is _start, which has no line information: addr2line searches
    // all of the executable's DWARF for it, for longer than the timeout.
    const int last = count >= 2 && in_main(count - 1) && !in_main(count - 2) ? count - 1 : count;
    int       symbols = 0;
    for (int i = 0; i < last; ++i) {
        if (!in_main(i)) {
            continue;
        }
        // A return address names the instruction after the call; one byte back is the call's line.
        const std::uintptr_t address = frames[i] - (i > 0 ? 1 : 0);
        addresses[symbols][0] = '\0';
#if defined(__APPLE__)
        AppendHex(addresses[symbols], sizeof addresses[symbols], address);
#else
        AppendHex(addresses[symbols], sizeof addresses[symbols], address - build.load_base);
#endif
        argv[argc++] = addresses[symbols++];
    }
    argv[argc] = nullptr;
    (void)load_arg;
    if (symbols == 0) {
        out.Str("no frames inside the executable\n");
        return;
    }
    out.Flush();

    const int saved_out = ::fcntl(1, F_DUPFD_CLOEXEC, 10);
    const int saved_err = ::fcntl(2, F_DUPFD_CLOEXEC, 10);
    const int null_fd = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    ::dup2(fd, 1);
    if (null_fd >= 0) {
        ::dup2(null_fd, 2);
        ::close(null_fd);
    }
    const pid_t child = ::vfork();
    if (child == 0) {
        ::execve(symbolizer, argv, environ);
        ::_exit(127);
    }
    if (saved_out >= 0) {
        ::dup2(saved_out, 1);
        ::close(saved_out);
    }
    if (saved_err >= 0) {
        ::dup2(saved_err, 2);
        ::close(saved_err);
    }
    if (child < 0) {
        out.Str("symbolization failed: could not start ").Str(symbolizer).Str("\n");
        return;
    }
    int status = 0;
    for (int waited = 0; waited < 1000; ++waited) {
        const pid_t done = ::waitpid(child, &status, WNOHANG);
        if (done == child) {
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                out.Str("(").Str(BaseName(symbolizer)).Str(" did not finish cleanly)\n");
            }
            return;
        }
        if (done < 0 && errno != EINTR) {
            return;
        }
        const timespec pause{0, 10'000'000};
        ::nanosleep(&pause, nullptr);
    }
    ::kill(child, SIGKILL);
    ::waitpid(child, &status, 0);
    out.Str("symbolization stopped after 10 s\n");
}

void Reraise(int signal, const siginfo_t *info) {
    struct sigaction action = previous[signal];
    if (action.sa_handler == SIG_IGN) {
        action.sa_handler = SIG_DFL;
    }
    ::sigaction(signal, &action, nullptr);
    // A fault re-executes its instruction once the handler returns; a sent signal has to be sent again.
    if (!info || info->si_code <= 0) {
        ::raise(signal);
    }
}

void OnSignal(int signal, siginfo_t *info, void *context) {
    if (handling.exchange(true)) {
        Reraise(signal, info);
        return;
    }
    const int saved_errno = errno;
    char      path[kPathSize + 64] = {};
    ReportPath(path, sizeof path, ".txt");
    int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    const bool to_file = fd >= 0;
    if (!to_file) {
        fd = LogTerminalFd();
    }

    std::uintptr_t frames[kMaxFrames];
    const auto     fault_pc = FaultPc(context);
    {
        Writer out(fd);
        char   detail[160] = " (";
        AppendDec(detail, sizeof detail, signal);
        Append(detail, sizeof detail, "), code ");
        AppendDec(detail, sizeof detail, info ? info->si_code : 0);
        const std::string_view code = info ? CodeName(signal, info->si_code) : "";
        if (!code.empty()) {
            Append(detail, sizeof detail, " ");
            Append(detail, sizeof detail, code);
        }
        const bool fault = signal != SIGABRT && info && info->si_code > 0;
        WriteHeader(out, SignalName(signal), detail, info ? reinterpret_cast<std::uintptr_t>(info->si_addr) : 0, fault);
        WriteRegisters(out, context);
        out.Flush();
        const int count = CollectFrames(fault_pc, frames);
        WriteFrames(out, frames, count);
        out.Flush();
        Symbolize(fd, frames, count);
        WriteTail(out, fd);
    }
    if (to_file) {
        ::close(fd);
        Announce(path);
    }
    errno = saved_errno;
    Reraise(signal, info);
}

#ifdef CT_ASAN
void OnSanitizerDeath() {
    LogDrainCaptured(-1);
    for (const int fd : {LogTerminalFd(), LogFileFd()}) {
        if (fd < 0) {
            continue;
        }
        {
            Writer out(fd);
            out.Str("== breadcrumbs at sanitizer abort ==\n");
        }
        BreadcrumbsWrite(fd);
    }
}
#endif

#if defined(__linux__)
int AddModule(dl_phdr_info *info, std::size_t, void *) {
    const int index = module_count.load(std::memory_order_relaxed);
    if (index >= kMaxModules) {
        return 1;
    }
    Module        &module = modules[index];
    std::uintptr_t low = UINTPTR_MAX;
    std::uintptr_t high = 0;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) &phdr = info->dlpi_phdr[i];
        if (phdr.p_type == PT_LOAD) {
            low = std::min<std::uintptr_t>(low, phdr.p_vaddr);
            high = std::max<std::uintptr_t>(high, phdr.p_vaddr + phdr.p_memsz);
        }
    }
    if (high == 0) {
        return 0;
    }
    module.base = info->dlpi_addr;
    module.start = info->dlpi_addr + low;
    module.end = info->dlpi_addr + high;
    CopyString(module.name, sizeof module.name, info->dlpi_name ? info->dlpi_name : "");
    module_count.store(index + 1, std::memory_order_release);
    return 0;
}
#endif

void FindSymbolizer() {
#if defined(__APPLE__)
    CopyString(symbolizer, sizeof symbolizer, ::access("/usr/bin/atos", X_OK) == 0 ? "/usr/bin/atos" : "");
#else
    const char *path = std::getenv("PATH");
    if (!path) {
        return;
    }
    for (const std::string_view name : {"llvm-symbolizer", "addr2line"}) {
        std::string_view rest = path;
        while (!rest.empty()) {
            const auto       colon = rest.find(':');
            std::string_view dir = rest.substr(0, colon);
            rest = colon == std::string_view::npos ? std::string_view() : rest.substr(colon + 1);
            if (dir.empty()) {
                continue;
            }
            const std::string candidate = std::string(dir) + "/" + std::string(name);
            if (::access(candidate.c_str(), X_OK) == 0) {
                CopyString(symbolizer, sizeof symbolizer, candidate);
                symbolizer_llvm = name == "llvm-symbolizer";
                return;
            }
        }
    }
#endif
}

void InstallHandlers() {
    stack_t stack{};
    stack.ss_sp = alternate_stack;
    stack.ss_size = sizeof alternate_stack;
    ::sigaltstack(&stack, nullptr);
    for (const int signal : kFatalSignals) {
#ifdef CT_ASAN
        // ASan reports these itself, with the allocation history this handler cannot give.
        if (signal == SIGSEGV || signal == SIGBUS || signal == SIGFPE) {
            continue;
        }
#endif
        struct sigaction action{};
        action.sa_sigaction = OnSignal;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        ::sigaction(signal, &action, &previous[signal]);
    }
#ifdef CT_ASAN
    __sanitizer_set_death_callback(OnSanitizerDeath);
#endif
}

#else // _WIN32

LPTOP_LEVEL_EXCEPTION_FILTER previous_filter = nullptr;

std::string_view ExceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_STACK_OVERFLOW:
            return "EXCEPTION_STACK_OVERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_IN_PAGE_ERROR:
            return "EXCEPTION_IN_PAGE_ERROR";
    }
    return "exception";
}

void WriteMinidump(EXCEPTION_POINTERS *pointers) {
    char path[kPathSize + 64] = {};
    ReportPath(path, sizeof path, ".dmp");
    HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
    if (!dbghelp) {
        return;
    }
    using WriteDump = BOOL(WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                     PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
    auto write_dump = reinterpret_cast<WriteDump>(reinterpret_cast<void *>(GetProcAddress(dbghelp, "MiniDumpWriteDump")));
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (write_dump && file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION exception{GetCurrentThreadId(), pointers, FALSE};
        write_dump(GetCurrentProcess(), GetCurrentProcessId(), file,
                   static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo), &exception,
                   nullptr, nullptr);
    }
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
}

LONG WINAPI OnException(EXCEPTION_POINTERS *pointers) {
    if (handling.exchange(true)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    char path[kPathSize + 64] = {};
    ReportPath(path, sizeof path, ".txt");
    int fd = _open(path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, 0644);
    const bool to_file = fd >= 0;
    if (!to_file) {
        fd = LogTerminalFd();
    }
    const EXCEPTION_RECORD *record = pointers->ExceptionRecord;
    {
        Writer out(fd);
        char   detail[64] = " (code ";
        AppendHex(detail, sizeof detail, record->ExceptionCode);
        Append(detail, sizeof detail, ")");
        std::uintptr_t address = reinterpret_cast<std::uintptr_t>(record->ExceptionAddress);
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            address = static_cast<std::uintptr_t>(record->ExceptionInformation[1]);
        }
        WriteHeader(out, ExceptionName(record->ExceptionCode), detail, address, true);
#if defined(_M_X64) || defined(__x86_64__)
        const CONTEXT *c = pointers->ContextRecord;
        out.Str("\n== registers ==\n");
        out.Str("rax ").Hex(c->Rax).Str("   rbx ").Hex(c->Rbx).Str("   rcx ").Hex(c->Rcx).Str("\n");
        out.Str("rdx ").Hex(c->Rdx).Str("   rsi ").Hex(c->Rsi).Str("   rdi ").Hex(c->Rdi).Str("\n");
        out.Str("rbp ").Hex(c->Rbp).Str("   rsp ").Hex(c->Rsp).Str("   rip ").Hex(c->Rip).Str("\n");
#endif
        std::uintptr_t frames[kMaxFrames];
        void          *raw[kMaxFrames];
        frames[0] = reinterpret_cast<std::uintptr_t>(record->ExceptionAddress);
        const USHORT captured = RtlCaptureStackBackTrace(0, kMaxFrames - 1, raw, nullptr);
        int          count = 1;
        for (USHORT i = 0; i < captured; ++i) {
            frames[count++] = reinterpret_cast<std::uintptr_t>(raw[i]);
        }
        WriteFrames(out, frames, count);
        out.Str("\n== symbols ==\nsee the .dmp file beside this report\n");
        WriteTail(out, fd);
    }
    if (to_file) {
        _close(fd);
    }
    WriteMinidump(pointers);
    if (to_file) {
        Announce(path);
    }
    return previous_filter ? previous_filter(pointers) : EXCEPTION_CONTINUE_SEARCH;
}

void AddWindowsModules() {
    // The executable alone: its frames are the ones that matter, and the minidump carries every module.
    Module &module = modules[0];
    module.base = BuildInfoGet().load_base;
    module.start = module.base;
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module.base);
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(module.base + dos->e_lfanew);
    module.end = module.base + nt->OptionalHeader.SizeOfImage;
    module_count.store(1);
}

#endif

void OnTerminate() {
    std::string text = "uncaught exception";
    if (const std::exception_ptr current = std::current_exception()) {
        std::string type = "unknown type";
#ifdef CT_HAVE_CXXABI
        if (const std::type_info *info = abi::__cxa_current_exception_type()) {
            int   status = 0;
            char *name = abi::__cxa_demangle(info->name(), nullptr, nullptr, &status);
            type = status == 0 && name ? name : info->name();
            std::free(name);
        }
#endif
        text = "uncaught exception " + type;
        try {
            std::rethrow_exception(current);
        } catch (const std::exception &error) {
            text += ": ";
            text += error.what();
        } catch (...) {
        }
    } else {
        text = "std::terminate called without an active exception";
    }
    CopyString(reason, sizeof reason, text);
    CT_LOG_ERROR("crash", "{}", text);
    std::abort();
}

[[gnu::noinline]] int Recurse(int depth) {
    volatile char frame[1024];
    frame[0] = static_cast<char>(depth);
    if (depth < 0) {
        return frame[0];
    }
    return Recurse(depth + 1) + frame[0];
}

} // namespace

void CrashSetReason(std::string_view text) {
    CopyString(reason, sizeof reason, text);
}

void CrashInstall(const CrashOptions &options) {
    std::error_code error;
    std::filesystem::create_directories(options.dir, error);
    LogRotate(options.dir, "crash-", ".txt", options.keep);
    LogRotate(options.dir, "crash-", ".dmp", options.keep);
    CopyString(report_dir, sizeof report_dir, PathsDisplay(options.dir));
    CopyString(session_log, sizeof session_log, PathsDisplay(LogSessionPath()));
    CopyString(command_line, sizeof command_line, options.command_line);
    CopyString(executable, sizeof executable, PathsDisplay(PathsExecutable()));
    symbolizer[0] = '\0';
    if (options.symbolize) {
#ifndef _WIN32
        FindSymbolizer();
#endif
    }
    BuildInfoGet();
    BreadcrumbGet(Crumb::Mode);

    const std::time_t now = std::time(nullptr);
    std::tm           local{};
#ifdef _WIN32
    localtime_s(&local, &now);
    std::tm utc{};
    gmtime_s(&utc, &now);
    utc_offset = static_cast<long>(std::mktime(&local) - std::mktime(&utc));
#else
    localtime_r(&now, &local);
    utc_offset = local.tm_gmtoff;
#endif

    CrashRefreshModules();
    std::set_terminate(OnTerminate);
#ifdef _WIN32
    previous_filter = SetUnhandledExceptionFilter(OnException);
#else
    InstallHandlers();
#endif
    installed.store(true);
    CT_LOG_INFO("crash", "crash reports go to {}{}", report_dir,
                symbolizer[0] ? "" : " (no symbolizer found: frames stay raw)");
}

bool CrashInstalled() {
    return installed.load();
}

void CrashRefreshModules() {
#if defined(__linux__)
    module_count.store(0, std::memory_order_release);
    dl_iterate_phdr(AddModule, nullptr);
#elif defined(__APPLE__)
    module_count.store(0, std::memory_order_release);
    const uint32_t count = std::min<uint32_t>(_dyld_image_count(), kMaxModules);
    for (uint32_t i = 0; i < count; ++i) {
        Module     &module = modules[i];
        const auto *header = reinterpret_cast<const mach_header_64 *>(_dyld_get_image_header(i));
        const auto  slide = static_cast<std::uintptr_t>(_dyld_get_image_vmaddr_slide(i));
        std::uintptr_t low = UINTPTR_MAX;
        std::uintptr_t high = 0;
        const auto    *command = reinterpret_cast<const load_command *>(header + 1);
        for (uint32_t c = 0; c < header->ncmds; ++c) {
            if (command->cmd == LC_SEGMENT_64) {
                const auto *segment = reinterpret_cast<const segment_command_64 *>(command);
                if (std::strcmp(segment->segname, "__PAGEZERO") != 0) {
                    low = std::min<std::uintptr_t>(low, segment->vmaddr);
                    high = std::max<std::uintptr_t>(high, segment->vmaddr + segment->vmsize);
                }
            }
            command = reinterpret_cast<const load_command *>(reinterpret_cast<const char *>(command) + command->cmdsize);
        }
        module.base = reinterpret_cast<std::uintptr_t>(header);
        module.start = high > low ? low + slide : module.base;
        module.end = high > low ? high + slide : module.base;
        CopyString(module.name, sizeof module.name, _dyld_get_image_name(i));
    }
    module_count.store(static_cast<int>(count), std::memory_order_release);
#elif defined(_WIN32)
    AddWindowsModules();
#endif
}

bool CrashTestKindValid(std::string_view kind) {
    return kind == "segv" || kind == "stack" || kind == "abort" || kind == "throw" || kind == "check";
}

void CrashTest(std::string_view kind) {
    BreadcrumbSetf(Crumb::Mode, "crash test %.*s", static_cast<int>(kind.size()), kind.data());
    CT_LOG_WARN("crash", "--crash-test {}: crashing on purpose", kind);
    if (kind == "segv") {
        // The value the 2026-10-18 crash dereferenced: non-canonical, so the kernel reports SI_KERNEL.
        *reinterpret_cast<volatile int *>(static_cast<std::uintptr_t>(0x000c001d00030004ull)) = 1;
    } else if (kind == "stack") {
        Recurse(0);
    } else if (kind == "throw") {
        throw std::runtime_error("bad scene");
    } else if (kind == "check") {
        const int *texture = nullptr;
        CT_CHECK(texture != nullptr, "crash test texture %d", 7);
    }
    std::abort();
}
