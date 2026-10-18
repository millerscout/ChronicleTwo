#include <gtest/gtest.h>

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/resource.h>
#include <unistd.h>

#include "mg_texture_port.hpp"
#include "platform/breadcrumbs.hpp"
#include "platform/build_info.hpp"
#include "platform/check.hpp"
#include "platform/crash.hpp"

namespace {

std::filesystem::path FreshDir(const std::string &name) {
    const auto dir = std::filesystem::temp_directory_path() / ("ct-crash-test-" + name + "-" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    return dir;
}

std::vector<std::filesystem::path> Reports(const std::filesystem::path &dir, std::string_view extension = ".txt") {
    std::vector<std::filesystem::path> reports;
    std::error_code                    error;
    for (const auto &entry : std::filesystem::directory_iterator(dir, error)) {
        if (entry.path().extension() == extension) {
            reports.push_back(entry.path());
        }
    }
    return reports;
}

std::string OnlyReport(const std::filesystem::path &dir) {
    const auto reports = Reports(dir);
    EXPECT_EQ(reports.size(), 1u);
    if (reports.empty()) {
        return {};
    }
    std::ifstream      in(reports.front());
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// Death-test children crash on purpose; their cores would be 500 MB each.
void NoCore() {
    const rlimit none{0, 0};
    ::setrlimit(RLIMIT_CORE, &none);
}

void Install(const std::filesystem::path &dir, bool symbolize = true) {
    NoCore();
    CrashInstall({.dir = dir, .command_line = "darkchronicle_tests", .symbolize = symbolize});
}

std::string Section(const std::string &report, const std::string &name) {
    const auto start = report.find("== " + name + " ==\n");
    if (start == std::string::npos) {
        return {};
    }
    const auto body = start + name.size() + 7;
    return report.substr(body, report.find("\n== ", body) - body);
}

using CrashDeathTest = ::testing::Test;

TEST_F(CrashDeathTest, SegvWritesEverySection) {
    const auto dir = FreshDir("segv");
    EXPECT_EXIT(
        {
            Install(dir, false);
            BreadcrumbSet(Crumb::Scene, "map 12");
            CrashTest("segv");
        },
        testing::KilledBySignal(SIGSEGV), "crash report: .*crash-[0-9]{8}-[0-9]{6}-[0-9]+\\.txt");
    const std::string report = OnlyReport(dir);
    EXPECT_NE(report.find("crash:        SIGSEGV (11)"), std::string::npos) << report;
    EXPECT_NE(report.find(std::string("build id:     ") + BuildInfoGet().build_id), std::string::npos) << report;
    for (const char *label : {"address:", "thread:", "load base:", "version:", "command line: darkchronicle_tests", "uptime:"}) {
        EXPECT_NE(report.find(label), std::string::npos) << label;
    }
    EXPECT_NE(Section(report, "registers").find("rax 0x000c001d00030004"), std::string::npos) << report;
    EXPECT_TRUE(std::regex_search(Section(report, "backtrace"), std::regex(R"(#0  0x[0-9a-f]{16}  darkchronicle_tests\+0x[0-9a-f]+)")))
        << report;
    EXPECT_NE(Section(report, "breadcrumbs").find("scene: map 12"), std::string::npos) << report;
    EXPECT_NE(Section(report, "recent log").find("--crash-test segv"), std::string::npos) << report;
    EXPECT_NE(report.find("== unflushed stderr =="), std::string::npos);
    EXPECT_NE(report.find("== end =="), std::string::npos);
}

TEST_F(CrashDeathTest, SymbolsNameTheCrashingFunction) {
    const auto dir = FreshDir("symbols");
    if (std::system("command -v llvm-symbolizer >/dev/null 2>&1 || command -v addr2line >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no symbolizer on PATH";
    }
    EXPECT_EXIT(
        {
            Install(dir);
            CrashTest("segv");
        },
        testing::KilledBySignal(SIGSEGV), "crash report: ");
    const std::string symbols = Section(OnlyReport(dir), "symbols");
    EXPECT_NE(symbols.find("CrashTest("), std::string::npos) << symbols;
    EXPECT_NE(symbols.find("crash.cpp:"), std::string::npos) << symbols;
    EXPECT_NE(symbols.find("crash_test.cpp:"), std::string::npos) << symbols;
}

TEST_F(CrashDeathTest, NoSymbolizerKeepsRawFrames) {
    const auto dir = FreshDir("no-symbolizer");
    EXPECT_EXIT(
        {
            ::setenv("PATH", "", 1);
            Install(dir);
            CrashTest("segv");
        },
        testing::KilledBySignal(SIGSEGV), "crash report: ");
    const std::string report = OnlyReport(dir);
    EXPECT_NE(Section(report, "symbols").find("symbolization skipped"), std::string::npos) << report;
    EXPECT_NE(Section(report, "backtrace").find("darkchronicle_tests+0x"), std::string::npos) << report;
}

TEST_F(CrashDeathTest, UncaughtExceptionIsNamed) {
    const auto dir = FreshDir("throw");
    EXPECT_EXIT(
        {
            Install(dir, false);
            []() noexcept { throw std::runtime_error("bad scene"); }();
        },
        testing::KilledBySignal(SIGABRT), "uncaught exception std::runtime_error: bad scene");
    const std::string report = OnlyReport(dir);
    EXPECT_NE(report.find("crash:        SIGABRT"), std::string::npos) << report;
    EXPECT_NE(report.find("reason:       uncaught exception std::runtime_error: bad scene"), std::string::npos) << report;
}

TEST_F(CrashDeathTest, StackOverflowIsReported) {
    const auto dir = FreshDir("stack");
    EXPECT_EXIT(
        {
            Install(dir, false);
            CrashTest("stack");
        },
        testing::KilledBySignal(SIGSEGV), "crash report: ");
    EXPECT_NE(OnlyReport(dir).find("crash:        SIGSEGV"), std::string::npos);
}

TEST_F(CrashDeathTest, FailedCheckReportsItsContext) {
    const auto dir = FreshDir("check");
    EXPECT_EXIT(
        {
            Install(dir, false);
            BreadcrumbSet(Crumb::Menu, "item menu");
            const int *texture = nullptr;
            CT_CHECK(texture != nullptr, "texture %d", 7);
        },
        testing::KilledBySignal(SIGABRT),
        "CT_CHECK\\(texture != nullptr\\) failed at .*crash_test\\.cpp:[0-9]+: texture 7(.|\n)*menu: item menu");
    const std::string report = OnlyReport(dir);
    EXPECT_NE(report.find("reason:       CT_CHECK(texture != nullptr) failed at"), std::string::npos) << report;
    EXPECT_NE(report.find("texture 7"), std::string::npos);
    EXPECT_NE(Section(report, "breadcrumbs").find("menu: item menu"), std::string::npos) << report;
}

TEST_F(CrashDeathTest, CorruptTexturePointerFailsTheCheck) {
    const auto dir = FreshDir("texture");
    EXPECT_EXIT(
        {
            Install(dir, false);
            MgTextureLookup(reinterpret_cast<const mgCTexture *>(0x000c001d00030004ull));
        },
        testing::KilledBySignal(SIGABRT), "MgTextureLookup");
    const std::string report = OnlyReport(dir);
    EXPECT_NE(report.find("CT_CHECK(ct::PlausiblePointer(texture))"), std::string::npos) << report;
    EXPECT_NE(report.find("MgTextureLookup"), std::string::npos) << report;
}

TEST_F(CrashDeathTest, WithoutTheHandlerNoReportIsWritten) {
    const auto dir = FreshDir("disabled");
    EXPECT_EXIT(
        {
            NoCore();
            CrashTest("segv");
        },
        testing::KilledBySignal(SIGSEGV), "");
    EXPECT_TRUE(Reports(dir).empty());
}

TEST(Crash, InstallKeepsTwentyReports) {
    const auto dir = FreshDir("rotate");
    std::filesystem::create_directories(dir);
    for (int i = 0; i < 21; ++i) {
        std::ofstream(dir / std::format("crash-20261018-1200{:02}-1.txt", i)) << "old";
    }
    EXPECT_EXIT(
        {
            Install(dir, false);
            std::exit(Reports(dir).size() == 20 && !std::filesystem::exists(dir / "crash-20261018-120000-1.txt") ? 0 : 1);
        },
        testing::ExitedWithCode(0), "");
}

TEST(Crash, PlausiblePointer) {
    int value = 0;
    EXPECT_TRUE(ct::PlausiblePointer(&value));
    EXPECT_FALSE(ct::PlausiblePointer(static_cast<const int *>(nullptr)));
    EXPECT_FALSE(ct::PlausiblePointer(reinterpret_cast<const int *>(0x10)));
    EXPECT_FALSE(ct::PlausiblePointer(reinterpret_cast<const int *>(0x000c001d00030004ull)));
    EXPECT_FALSE(ct::PlausiblePointer(reinterpret_cast<const int *>(reinterpret_cast<std::uintptr_t>(&value) + 1)));
    EXPECT_TRUE(ct::PlausiblePointer(reinterpret_cast<const void *>(reinterpret_cast<std::uintptr_t>(&value) + 1)));
}

} // namespace
