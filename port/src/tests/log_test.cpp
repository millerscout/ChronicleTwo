#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include "platform/log.hpp"

namespace {

std::filesystem::path FreshDir(const std::string &name) {
    const auto dir = std::filesystem::temp_directory_path() / ("ct-log-test-" + name + "-" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream      in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void Touch(const std::filesystem::path &path) {
    std::ofstream(path) << "x";
}

class Log : public ::testing::Test {
  protected:
    void TearDown() override {
        LogShutdown();
        LogSetLevel(LogLevel::Info);
    }
};

TEST_F(Log, ParsesEveryLevelName) {
    EXPECT_EQ(LogParseLevel("error"), LogLevel::Error);
    EXPECT_EQ(LogParseLevel("warn"), LogLevel::Warn);
    EXPECT_EQ(LogParseLevel("info"), LogLevel::Info);
    EXPECT_EQ(LogParseLevel("debug"), LogLevel::Debug);
    EXPECT_EQ(LogParseLevel("trace"), LogLevel::Trace);
    EXPECT_EQ(LogParseLevel("loud"), std::nullopt);
    EXPECT_EQ(LogParseLevel(""), std::nullopt);
}

TEST_F(Log, DropsLinesBelowTheLevel) {
    LogSetLevel(LogLevel::Debug);
    CT_LOG_DEBUG("test", "debug line kept");
    CT_LOG_TRACE("test", "trace line dropped");
    const auto lines = LogRecentLines();
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines.back().find("debug line kept"), std::string::npos);
    for (const std::string &line : lines) {
        EXPECT_EQ(line.find("trace line dropped"), std::string::npos);
    }
}

TEST_F(Log, LineCarriesTimeLevelCategoryAndThread) {
    CT_LOG_INFO("scene", "loading map {}", 3);
    const std::string line = LogRecentLines().back();
    EXPECT_TRUE(std::regex_search(line, std::regex(R"(^\[\s*\d+\.\d{3}\] INFO\s+scene\s+main\s+loading map 3$)"))) << line;
}

TEST_F(Log, RingKeepsTheLatestLines) {
    for (int i = 0; i < 300; ++i) {
        CT_LOG_INFO("ring", "line {}", i);
    }
    const auto lines = LogRecentLines();
    ASSERT_GE(lines.size(), 200u);
    EXPECT_NE(lines.back().find("line 299"), std::string::npos);
    EXPECT_NE(lines[lines.size() - 200].find("line 100"), std::string::npos);
}

TEST_F(Log, SessionFileStartsWithItsHeader) {
    const auto dir = FreshDir("header");
    ASSERT_TRUE(LogOpenSession({.dir = dir, .capture_stderr = false, .command_line = "darkchronicle --headless"}));
    const auto path = LogSessionPath();
    EXPECT_EQ(path.parent_path(), dir);
    EXPECT_TRUE(std::regex_match(path.filename().string(), std::regex(R"(session-\d{8}-\d{6}-\d+\.log)"))) << path;
    CT_LOG_ERROR("test", "an error line");
    const std::string text = ReadFile(path);
    EXPECT_NE(text.find("build id "), std::string::npos);
    EXPECT_NE(text.find("version "), std::string::npos);
    EXPECT_NE(text.find("platform "), std::string::npos);
    EXPECT_NE(text.find("command line darkchronicle --headless"), std::string::npos);
    // Every line is written through at once, so an error is on disk before LogShutdown.
    EXPECT_NE(text.find("an error line"), std::string::npos);
}

TEST_F(Log, UnwritableDirectoryFallsBackToStderr) {
    const auto dir = FreshDir("unwritable");
    Touch(dir / "file");
    testing::internal::CaptureStderr();
    EXPECT_FALSE(LogOpenSession({.dir = dir / "file" / "logs", .capture_stderr = false}));
    const std::string warning = testing::internal::GetCapturedStderr();
    EXPECT_NE(warning.find("cannot create the session log"), std::string::npos) << warning;
    EXPECT_NE(warning.find("file/logs"), std::string::npos) << warning;
    EXPECT_TRUE(LogSessionPath().empty());
}

TEST_F(Log, CapturedStderrReachesTheFileAndTheTerminal) {
    const auto dir = FreshDir("capture");
    // Stand in for the terminal: fd 2 points at a file before the session takes it over.
    const auto terminal_path = dir / "terminal.txt";
    const int  terminal = ::open(terminal_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ASSERT_GE(terminal, 0);
    const int saved = ::dup(2);
    ::dup2(terminal, 2);
    ::close(terminal);

    ASSERT_TRUE(LogOpenSession({.dir = dir / "logs"}));
    std::fprintf(stderr, "Vulkan validation: a third-party line\n");
    LogFlush();
    const auto session = LogSessionPath();
    LogShutdown();
    ::dup2(saved, 2);
    ::close(saved);

    const std::string file = ReadFile(session);
    EXPECT_TRUE(std::regex_search(file, std::regex(R"(OUT\s+stderr\s+-\s+Vulkan validation: a third-party line)"))) << file;
    EXPECT_NE(ReadFile(terminal_path).find("Vulkan validation: a third-party line\n"), std::string::npos);
}

TEST_F(Log, RotationKeepsTheNewestAndLeavesOtherFiles) {
    const auto dir = FreshDir("rotate");
    for (int i = 0; i < 11; ++i) {
        Touch(dir / std::format("session-20261018-1200{:02}-1.log", i));
    }
    Touch(dir / "notes.txt");
    Touch(dir / "crash-20261018-120000-1.txt");
    EXPECT_EQ(LogRotate(dir, "session-", ".log", 10), 1u);
    EXPECT_FALSE(std::filesystem::exists(dir / "session-20261018-120000-1.log"));
    EXPECT_TRUE(std::filesystem::exists(dir / "session-20261018-120010-1.log"));
    EXPECT_TRUE(std::filesystem::exists(dir / "notes.txt"));
    EXPECT_TRUE(std::filesystem::exists(dir / "crash-20261018-120000-1.txt"));
}

TEST_F(Log, OpeningASessionRotatesToTen) {
    const auto dir = FreshDir("rotate-open");
    for (int i = 0; i < 10; ++i) {
        Touch(dir / std::format("session-20000101-0000{:02}-1.log", i));
    }
    ASSERT_TRUE(LogOpenSession({.dir = dir, .capture_stderr = false}));
    std::size_t count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        count += entry.path().filename().string().starts_with("session-");
    }
    EXPECT_EQ(count, 10u);
    EXPECT_FALSE(std::filesystem::exists(dir / "session-20000101-000000-1.log"));
    EXPECT_TRUE(std::filesystem::exists(LogSessionPath()));
}

TEST_F(Log, SizeCapKeepsOnlyWarningsAndErrors) {
    const auto dir = FreshDir("cap");
    ASSERT_TRUE(LogOpenSession({.dir = dir, .capture_stderr = false, .max_bytes = 1}));
    CT_LOG_INFO("test", "info after the cap");
    CT_LOG_WARN("test", "warning after the cap");
    const std::string text = ReadFile(LogSessionPath());
    EXPECT_EQ(text.find("info after the cap"), std::string::npos);
    EXPECT_NE(text.find("warning after the cap"), std::string::npos);
    EXPECT_NE(text.find("size cap"), std::string::npos);
}

} // namespace
