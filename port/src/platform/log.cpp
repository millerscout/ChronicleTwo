#include "platform/log.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <system_error>
#include <thread>

#include "platform/build_info.hpp"
#include "platform/paths.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif
#endif

namespace {

#ifdef _WIN32
int  FdWrite(int fd, const void *data, std::size_t size) { return _write(fd, data, static_cast<unsigned>(size)); }
int  FdRead(int fd, void *data, std::size_t size) { return _read(fd, data, static_cast<unsigned>(size)); }
int  FdDup(int fd) { return _dup(fd); }
int  FdDup2(int from, int to) { return _dup2(from, to); }
int  FdClose(int fd) { return _close(fd); }
long ProcessId() { return _getpid(); }
#else
int  FdWrite(int fd, const void *data, std::size_t size) { return static_cast<int>(::write(fd, data, size)); }
int  FdRead(int fd, void *data, std::size_t size) { return static_cast<int>(::read(fd, data, size)); }
int  FdDup(int fd) { return ::fcntl(fd, F_DUPFD_CLOEXEC, 3); }
int  FdDup2(int from, int to) { return ::dup2(from, to); }
int  FdClose(int fd) { return ::close(fd); }
long ProcessId() { return static_cast<long>(::getpid()); }
#endif

void WriteAll(int fd, const char *data, std::size_t size) {
    while (size > 0 && fd >= 0) {
        const int written = FdWrite(fd, data, size);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        data += written;
        size -= static_cast<std::size_t>(written);
    }
}

constexpr std::size_t kRingSlots = 256;
constexpr std::size_t kRingLine = 256;

struct RingSlot {
    std::atomic<std::uint32_t> length{0};
    char                       text[kRingLine];
};

std::array<RingSlot, kRingSlots> ring;
std::atomic<std::uint64_t>       ring_head{0};

std::atomic<int>           level{static_cast<int>(LogLevel::Info)};
std::atomic<int>           file_fd{-1};
std::atomic<int>           terminal_fd{2};
std::atomic<int>           pipe_read{-1};
std::atomic<bool>          capturing{false};
std::atomic<bool>          stop_reader{false};
std::atomic<std::uint64_t> file_bytes{0};
std::atomic<bool>          cap_noted{false};
std::uint64_t              max_bytes = 64ull << 20;
std::filesystem::path      session_path;
std::thread                reader;
const auto                 start_time = std::chrono::steady_clock::now();

void RingPush(const char *line, std::size_t size) {
    while (size > 0 && (line[size - 1] == '\n' || line[size - 1] == '\r')) {
        --size;
    }
    RingSlot &slot = ring[ring_head.fetch_add(1, std::memory_order_relaxed) % kRingSlots];
    size = std::min(size, kRingLine);
    slot.length.store(0, std::memory_order_relaxed);
    std::memcpy(slot.text, line, size);
    slot.length.store(static_cast<std::uint32_t>(size), std::memory_order_release);
}

double Elapsed() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
}

std::string_view LevelLabel(LogLevel value) {
    switch (value) {
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Trace:
            return "TRACE";
    }
    return "?";
}

// The thread's name when it was given one, "main" for the main thread, otherwise t<id>.
const char *ThreadLabel() {
    thread_local char label[24] = {};
    if (label[0] != '\0') {
        return label;
    }
#if defined(__linux__)
    const long tid = ::syscall(SYS_gettid);
    if (tid == ProcessId()) {
        std::snprintf(label, sizeof label, "main");
        return label;
    }
    char name[16] = {};
    if (pthread_getname_np(pthread_self(), name, sizeof name) == 0 && name[0] != '\0' &&
        std::strcmp(name, program_invocation_short_name) != 0) {
        std::snprintf(label, sizeof label, "%s", name);
    } else {
        std::snprintf(label, sizeof label, "t%ld", tid);
    }
#elif defined(__APPLE__)
    std::uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    if (pthread_main_np()) {
        std::snprintf(label, sizeof label, "main");
    } else {
        std::snprintf(label, sizeof label, "t%llu", static_cast<unsigned long long>(tid));
    }
#elif defined(_WIN32)
    std::snprintf(label, sizeof label, "t%lu", static_cast<unsigned long>(GetCurrentThreadId()));
#endif
    return label;
}

// Writes a formatted line to the file, keeping to the size cap.
void FileLine(const char *line, std::size_t size, bool important) {
    const int fd = file_fd.load(std::memory_order_acquire);
    if (fd < 0) {
        return;
    }
    if (file_bytes.load(std::memory_order_relaxed) >= max_bytes && !important) {
        if (!cap_noted.exchange(true)) {
            constexpr std::string_view note = "-- log reached its size cap: only warn and error lines follow --\n";
            WriteAll(fd, note.data(), note.size());
        }
        return;
    }
    file_bytes.fetch_add(size, std::memory_order_relaxed);
    WriteAll(fd, line, size);
}

// A captured stderr line: the terminal already has it, so it goes to the file and the ring.
void CapturedLine(std::string_view text) {
    char buffer[kRingLine + 64];
    int  size = std::snprintf(buffer, sizeof buffer, "[%10.3f] %-5s %-8s %-12s %.*s\n", Elapsed(), "OUT", "stderr", "-",
                              static_cast<int>(std::min<std::size_t>(text.size(), kRingLine)), text.data());
    size = std::clamp(size, 0, static_cast<int>(sizeof buffer) - 1);
    // A cut line still ends the file's line.
    buffer[size - 1] = '\n';
    RingPush(buffer, static_cast<std::size_t>(size));
    FileLine(buffer, static_cast<std::size_t>(size), false);
}

void RestoreStderr() {
    if (capturing.exchange(false)) {
        FdDup2(terminal_fd.load(), 2);
    }
}

#ifndef _WIN32
// Drains what is in the pipe now; returns false at end of file.
bool ReadPipe(std::string &partial) {
    char buffer[4096];
    for (;;) {
        const int count = FdRead(pipe_read.load(), buffer, sizeof buffer);
        if (count > 0) {
            WriteAll(terminal_fd.load(), buffer, static_cast<std::size_t>(count));
            partial.append(buffer, static_cast<std::size_t>(count));
            std::size_t start = 0;
            for (std::size_t newline; (newline = partial.find('\n', start)) != std::string::npos; start = newline + 1) {
                CapturedLine(std::string_view(partial).substr(start, newline - start));
            }
            partial.erase(0, start);
            continue;
        }
        if (count == 0) {
            return false;
        }
        return errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK;
    }
}

void ReaderLoop() {
    std::string partial;
    try {
        for (;;) {
            pollfd entry{pipe_read.load(), POLLIN, 0};
            const int ready = ::poll(&entry, 1, 100);
            if (ready > 0 && !ReadPipe(partial)) {
                break;
            }
            if (ready == 0 && stop_reader.load()) {
                break;
            }
        }
    } catch (...) {
    }
    if (!partial.empty()) {
        CapturedLine(partial);
    }
    RestoreStderr();
}
#else
void ReaderLoop() {
    std::string partial;
    char        buffer[4096];
    for (;;) {
        const int count = FdRead(pipe_read.load(), buffer, sizeof buffer);
        if (count <= 0) {
            break;
        }
        WriteAll(terminal_fd.load(), buffer, static_cast<std::size_t>(count));
        partial.append(buffer, static_cast<std::size_t>(count));
        std::size_t start = 0;
        for (std::size_t newline; (newline = partial.find('\n', start)) != std::string::npos; start = newline + 1) {
            CapturedLine(std::string_view(partial).substr(start, newline - start));
        }
        partial.erase(0, start);
    }
    if (!partial.empty()) {
        CapturedLine(partial);
    }
    RestoreStderr();
}
#endif

bool StartCapture() {
    std::fflush(stderr);
    const int terminal = FdDup(2);
    if (terminal < 0) {
        return false;
    }
    int fds[2];
#ifdef _WIN32
    if (_pipe(fds, 1 << 20, _O_BINARY | _O_NOINHERIT) != 0) {
        FdClose(terminal);
        return false;
    }
#else
    if (::pipe(fds) != 0) {
        FdClose(terminal);
        return false;
    }
    ::fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(fds[0], F_SETFL, ::fcntl(fds[0], F_GETFL) | O_NONBLOCK);
#if defined(__linux__)
    ::fcntl(fds[1], F_SETPIPE_SZ, 1 << 20);
#endif
#endif
    if (FdDup2(fds[1], 2) < 0) {
        FdClose(fds[0]);
        FdClose(fds[1]);
        FdClose(terminal);
        return false;
    }
    FdClose(fds[1]);
    terminal_fd.store(terminal);
    pipe_read.store(fds[0]);
    stop_reader.store(false);
    capturing.store(true);
    reader = std::thread(ReaderLoop);
    return true;
}

std::string SessionFileName() {
    const std::time_t now = std::time(nullptr);
    std::tm           local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char name[64];
    std::strftime(name, sizeof name, "session-%Y%m%d-%H%M%S", &local);
    return std::format("{}-{}.log", name, ProcessId());
}

} // namespace

std::optional<LogLevel> LogParseLevel(std::string_view text) {
    constexpr std::pair<std::string_view, LogLevel> kNames[] = {
        {"error", LogLevel::Error}, {"warn", LogLevel::Warn},   {"warning", LogLevel::Warn},
        {"info", LogLevel::Info},   {"debug", LogLevel::Debug}, {"trace", LogLevel::Trace},
    };
    for (const auto &[name, value] : kNames) {
        if (text == name) {
            return value;
        }
    }
    return std::nullopt;
}

void LogSetLevel(LogLevel value) {
    level.store(static_cast<int>(value), std::memory_order_relaxed);
}

LogLevel LogGetLevel() {
    return static_cast<LogLevel>(level.load(std::memory_order_relaxed));
}

bool LogEnabled(LogLevel value) {
    return static_cast<int>(value) <= level.load(std::memory_order_relaxed);
}

void LogWrite(LogLevel value, std::string_view category, std::string_view text) {
    if (!LogEnabled(value)) {
        return;
    }
    char line[1200];
    int  size = std::snprintf(line, sizeof line, "[%10.3f] %-5.*s %-8.*s %-12s %.*s\n", Elapsed(),
                              static_cast<int>(LevelLabel(value).size()), LevelLabel(value).data(),
                              static_cast<int>(category.size()), category.data(), ThreadLabel(),
                              static_cast<int>(std::min<std::size_t>(text.size(), 1024)), text.data());
    size = std::clamp(size, 1, static_cast<int>(sizeof line) - 1);
    line[size - 1] = '\n';
    RingPush(line, static_cast<std::size_t>(size));
    FileLine(line, static_cast<std::size_t>(size), value <= LogLevel::Warn);

    // The terminal keeps the "category: text" lines the port printed before it had a log.
    char terminal[1200];
    int  terminal_size = std::snprintf(terminal, sizeof terminal, "%.*s: %.*s\n", static_cast<int>(category.size()),
                                       category.data(), static_cast<int>(std::min<std::size_t>(text.size(), 1024)),
                                       text.data());
    terminal_size = std::clamp(terminal_size, 1, static_cast<int>(sizeof terminal) - 1);
    terminal[terminal_size - 1] = '\n';
    WriteAll(terminal_fd.load(std::memory_order_relaxed), terminal, static_cast<std::size_t>(terminal_size));
}

bool LogOpenSession(const LogSessionOptions &options) {
    max_bytes = options.max_bytes;
    session_path.clear();
    std::error_code error;
    std::filesystem::create_directories(options.dir, error);
    const std::filesystem::path path = options.dir / SessionFileName();
#ifdef _WIN32
    const int fd = _wopen(path.c_str(), _O_WRONLY | _O_CREAT | _O_APPEND | _O_BINARY | _O_NOINHERIT, 0644);
#else
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
#endif
    if (fd < 0) {
        const std::string reason = std::generic_category().message(errno);
        std::fprintf(stderr, "warning: cannot create the session log %s (%s); logging to stderr only\n",
                     PathsDisplay(path).c_str(), reason.c_str());
        return false;
    }
    session_path = path;
    file_bytes.store(0);
    cap_noted.store(false);
    file_fd.store(fd, std::memory_order_release);
    LogRotate(options.dir, "session-", ".log", options.keep, path);

    const BuildInfo &build = BuildInfoGet();
    CT_LOG_INFO("log", "session log {}", PathsDisplay(path));
    CT_LOG_INFO("log", "build id {}", build.build_id[0] ? build.build_id : "(none)");
    CT_LOG_INFO("log", "version {}", build.version);
    CT_LOG_INFO("log", "platform {}, pid {}", build.platform, ProcessId());
    CT_LOG_INFO("log", "command line {}", options.command_line);

    if (options.capture_stderr) {
        StartCapture();
    }
    std::atexit(LogShutdown);
    return true;
}

const std::filesystem::path &LogSessionPath() {
    return session_path;
}

void LogFlush() {
    std::fflush(stderr);
#ifndef _WIN32
    if (!capturing.load()) {
        return;
    }
    for (int wait = 0; wait < 50; ++wait) {
        int pending = 0;
        if (::ioctl(pipe_read.load(), FIONREAD, &pending) != 0 || pending == 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // FIONREAD reaches 0 when the reader has taken the bytes; let it write them out.
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
#endif
}

void LogShutdown() {
    std::fflush(stderr);
    if (reader.joinable()) {
        // fd 2 goes back first: a child that inherited the pipe would otherwise hold the reader open.
        RestoreStderr();
        stop_reader.store(true);
        reader.join();
    }
    if (const int fd = pipe_read.exchange(-1); fd >= 0) {
        FdClose(fd);
    }
    if (const int fd = terminal_fd.exchange(2); fd != 2) {
        FdClose(fd);
    }
    if (const int fd = file_fd.exchange(-1); fd >= 0) {
        FdClose(fd);
    }
    session_path.clear();
}

std::size_t LogRotate(const std::filesystem::path &dir, std::string_view prefix, std::string_view suffix,
                      std::size_t keep, const std::filesystem::path &exclude) {
    std::vector<std::filesystem::path> files;
    std::error_code                    error;
    for (const auto &entry : std::filesystem::directory_iterator(dir, error)) {
        const std::string name = PathsDisplay(entry.path().filename());
        if (entry.is_regular_file(error) && name.starts_with(prefix) && name.ends_with(suffix) && entry.path() != exclude) {
            files.push_back(entry.path());
        }
    }
    const std::size_t keep_others = exclude.empty() ? keep : (keep > 0 ? keep - 1 : 0);
    if (files.size() <= keep_others) {
        return 0;
    }
    std::sort(files.begin(), files.end());
    const std::size_t remove = files.size() - keep_others;
    std::size_t       removed = 0;
    for (std::size_t i = 0; i < remove; ++i) {
        if (std::filesystem::remove(files[i], error)) {
            ++removed;
        }
    }
    return removed;
}

std::vector<std::string> LogRecentLines() {
    std::vector<std::string> lines;
    const std::uint64_t      head = ring_head.load(std::memory_order_acquire);
    for (std::uint64_t index = head > kRingSlots ? head - kRingSlots : 0; index < head; ++index) {
        const RingSlot &slot = ring[index % kRingSlots];
        lines.emplace_back(slot.text, slot.length.load(std::memory_order_acquire));
    }
    return lines;
}

int LogTerminalFd() {
    return terminal_fd.load(std::memory_order_relaxed);
}

int LogFileFd() {
    return file_fd.load(std::memory_order_relaxed);
}

void LogDrainCaptured(int copy_fd) {
#ifndef _WIN32
    const int fd = pipe_read.load(std::memory_order_relaxed);
    if (fd < 0) {
        return;
    }
    char buffer[4096];
    for (int rounds = 0; rounds < 256; ++rounds) {
        const ssize_t count = ::read(fd, buffer, sizeof buffer);
        if (count <= 0) {
            break;
        }
        WriteAll(terminal_fd.load(std::memory_order_relaxed), buffer, static_cast<std::size_t>(count));
        WriteAll(file_fd.load(std::memory_order_relaxed), buffer, static_cast<std::size_t>(count));
        WriteAll(copy_fd, buffer, static_cast<std::size_t>(count));
    }
#else
    (void)copy_fd;
#endif
}

void LogWriteRecent(int fd) {
    const std::uint64_t head = ring_head.load(std::memory_order_acquire);
    for (std::uint64_t index = head > kRingSlots ? head - kRingSlots : 0; index < head; ++index) {
        const RingSlot     &slot = ring[index % kRingSlots];
        const std::uint32_t length = slot.length.load(std::memory_order_acquire);
        WriteAll(fd, slot.text, std::min<std::size_t>(length, kRingLine));
        WriteAll(fd, "\n", 1);
    }
}
