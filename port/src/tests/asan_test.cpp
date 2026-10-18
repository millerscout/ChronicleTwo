#include <gtest/gtest.h>

#if defined(__SANITIZE_ADDRESS__)

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <sys/resource.h>
#include <sys/wait.h>

#include "platform/breadcrumbs.hpp"
#include "platform/crash.hpp"
#include "platform/log.hpp"

namespace {

std::filesystem::path FreshDir() {
    const auto dir = std::filesystem::temp_directory_path() / ("ct-asan-test-" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    return dir;
}

bool AsanDied(int status) {
    return (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
}

std::string ReadLogs(const std::filesystem::path &dir) {
    std::string text;
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(dir, error)) {
        std::ifstream in(entry.path());
        std::ostringstream part;
        part << in.rdbuf();
        text += part.str();
    }
    return text;
}

// ASan owns SIGSEGV. The death callback still has to put the breadcrumbs in the session log.
TEST(AsanDeathTest, OutOfBoundsReadReachesTheSessionLog) {
    const auto dir = FreshDir();
    const auto logs = dir / "logs";
    EXPECT_EXIT(
        {
            rlimit none{};
            none.rlim_cur = 0;
            none.rlim_max = 0;
            ::setrlimit(RLIMIT_CORE, &none);
            LogOpenSession({.dir = logs, .capture_stderr = true, .command_line = "asan-test"});
            CrashInstall({.dir = dir / "crashes", .command_line = "asan-test", .symbolize = false});
            BreadcrumbSet(Crumb::Scene, "asan room");
            auto          *values = new int[4];
            volatile int   past = values[4];
            (void)past;
        },
        AsanDied,
        "");
    const std::string log = ReadLogs(logs);
    EXPECT_NE(log.find("AddressSanitizer"), std::string::npos) << log;
    EXPECT_NE(log.find("asan room"), std::string::npos) << log;
}

} // namespace

#endif
