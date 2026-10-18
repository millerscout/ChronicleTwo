#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

// Crash reports (docs/PC.md, Logs and crash reports): a fatal signal, an uncaught exception or a failed CT_CHECK
// writes crashes/crash-<time>-<pid>.txt under the save root, symbolized against the image the process has mapped,
// then dies the way it would have without the handler, so core dumps and debuggers see the same signal.

struct CrashOptions {
    std::filesystem::path dir;        // the crashes/ directory, created when missing
    std::size_t           keep = 20;  // reports kept, counting none of this run's
    std::string           command_line;
    bool                  symbolize = true;
};

void CrashInstall(const CrashOptions &options);
bool CrashInstalled();

// A line the next crash report states as its reason (a failed check, an uncaught exception).
void CrashSetReason(std::string_view text);

// Re-reads the loaded modules, so frames in libraries loaded after CrashInstall (Vulkan drivers) get a name.
void CrashRefreshModules();

// The --crash-test kinds: segv, stack, abort, throw, check.
bool CrashTestKindValid(std::string_view kind);
[[noreturn]] void CrashTest(std::string_view kind);
