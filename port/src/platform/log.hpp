#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The session log (docs/PC.md, Logs and crash reports): logs/session-<time>-<pid>.log under the save root. Lines
// reach the file with one write each, so nothing is lost when the process dies. Whatever the process writes to
// stderr (Kazarov, Vulkan, SDL, plain fprintf) is copied into the file as well while a session is open.

enum class LogLevel : int {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
    Trace = 4,
};

inline constexpr std::string_view kLogLevelNames = "error, warn, info, debug, trace";

std::optional<LogLevel> LogParseLevel(std::string_view text);
void                    LogSetLevel(LogLevel level);
LogLevel                LogGetLevel();
bool                    LogEnabled(LogLevel level);

// Writes one line to the terminal, the session file and the recent-lines ring. Does not allocate.
void LogWrite(LogLevel level, std::string_view category, std::string_view text);

// Formats into a stack buffer (lines past 1 KiB are cut) and writes it.
template <class... Args>
void LogFormat(LogLevel level, std::string_view category, std::format_string<Args...> format, Args &&...args) {
    char       buffer[1024];
    const auto result = std::format_to_n(buffer, sizeof buffer, format, std::forward<Args>(args)...);
    LogWrite(level, category, std::string_view(buffer, std::min<std::size_t>(result.size, sizeof buffer)));
}

#define CT_LOG(level, category, ...)                       \
    do {                                                   \
        if (LogEnabled(level)) {                           \
            LogFormat(level, category, __VA_ARGS__);       \
        }                                                  \
    } while (0)
#define CT_LOG_ERROR(category, ...) CT_LOG(LogLevel::Error, category, __VA_ARGS__)
#define CT_LOG_WARN(category, ...)  CT_LOG(LogLevel::Warn, category, __VA_ARGS__)
#define CT_LOG_INFO(category, ...)  CT_LOG(LogLevel::Info, category, __VA_ARGS__)
#define CT_LOG_DEBUG(category, ...) CT_LOG(LogLevel::Debug, category, __VA_ARGS__)
#define CT_LOG_TRACE(category, ...) CT_LOG(LogLevel::Trace, category, __VA_ARGS__)

struct LogSessionOptions {
    std::filesystem::path dir;                  // the logs/ directory, created when missing
    std::size_t           keep = 10;            // session logs kept, counting the new one
    bool                  capture_stderr = true;
    std::uint64_t         max_bytes = 64ull << 20; // past this only warn and error lines are written
    std::string           command_line;
};

// Creates the session file, rotates old ones and starts copying stderr into it. Returns false, after one warning
// on stderr, when the file cannot be created: logging then goes to stderr alone.
bool LogOpenSession(const LogSessionOptions &options);
const std::filesystem::path &LogSessionPath();
// Waits (briefly) until what was written to stderr so far has reached the file.
void LogFlush();
// Drains captured stderr, gives fd 2 back to the terminal and closes the file. Runs at exit; safe to call twice.
void LogShutdown();

// Deletes the oldest files named <prefix>*<suffix> in dir until at most keep remain, never exclude. Returns how
// many were deleted.
std::size_t LogRotate(const std::filesystem::path &dir, std::string_view prefix, std::string_view suffix,
                      std::size_t keep, const std::filesystem::path &exclude = {});

// The recent-lines ring, oldest first.
std::vector<std::string> LogRecentLines();

// Async-signal-safe, for the crash handler.
int  LogTerminalFd();               // the terminal's stderr (2 when stderr is not captured)
int  LogFileFd();                   // the session file, -1 when none is open
void LogDrainCaptured(int copy_fd); // moves unread captured stderr to the file, the terminal and copy_fd
void LogWriteRecent(int fd);        // the ring, oldest first, one line each
