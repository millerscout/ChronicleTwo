#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#include <unistd.h>

#include "platform/breadcrumbs.hpp"
#include "platform/log.hpp"

namespace {

class Breadcrumbs : public ::testing::Test {
  protected:
    void TearDown() override {
        BreadcrumbsClear();
        LogSetLevel(LogLevel::Info);
    }
};

TEST_F(Breadcrumbs, KeepsTheLatestValue) {
    BreadcrumbSet(Crumb::Mode, "edit");
    BreadcrumbSetf(Crumb::Scene, "map %d", 12);
    EXPECT_EQ(BreadcrumbGet(Crumb::Mode), "edit");
    EXPECT_EQ(BreadcrumbGet(Crumb::Scene), "map 12");
    BreadcrumbSet(Crumb::Mode, "dungeon");
    EXPECT_EQ(BreadcrumbGet(Crumb::Mode), "dungeon");
    BreadcrumbSet(Crumb::Mode, "");
    EXPECT_EQ(BreadcrumbGet(Crumb::Mode), "");
}

TEST_F(Breadcrumbs, CutsLongValues) {
    const std::string longer(200, 'x');
    BreadcrumbSet(Crumb::Asset, longer);
    EXPECT_EQ(BreadcrumbGet(Crumb::Asset), std::string(kCrumbValueSize - 1, 'x'));
}

TEST_F(Breadcrumbs, LogsOnlyChangesAtDebug) {
    LogSetLevel(LogLevel::Debug);
    BreadcrumbSet(Crumb::Menu, "item menu");
    const auto first = LogRecentLines().size();
    BreadcrumbSet(Crumb::Menu, "item menu");
    EXPECT_EQ(LogRecentLines().size(), first);
    EXPECT_NE(LogRecentLines().back().find("menu = item menu"), std::string::npos);
}

TEST_F(Breadcrumbs, WritesOneLinePerCrumb) {
    BreadcrumbSet(Crumb::Mode, "edit walk");
    BreadcrumbSetf(Crumb::Floor, "dungeon %d floor %d", 2, 5);
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    BreadcrumbsWrite(fds[1]);
    ::close(fds[1]);
    std::string text;
    char        buffer[512];
    for (ssize_t count; (count = ::read(fds[0], buffer, sizeof buffer)) > 0;) {
        text.append(buffer, static_cast<std::size_t>(count));
    }
    ::close(fds[0]);
    EXPECT_EQ(text, "mode: edit walk\nscene: \nmenu: \nfloor: dungeon 2 floor 5\nasset: \n");
}

TEST_F(Breadcrumbs, ReaderNeverSeesAHalfWrittenValue) {
    const std::string a(kCrumbValueSize - 1, 'a');
    const std::string b(kCrumbValueSize - 1, 'b');
    BreadcrumbSet(Crumb::Scene, a);
    std::atomic<bool> stop{false};
    std::thread       writer([&] {
        for (int i = 0; !stop.load(); ++i) {
            BreadcrumbSet(Crumb::Scene, i % 2 ? a : b);
        }
    });
    int torn = 0;
    for (int i = 0; i < 200000; ++i) {
        const std::string value = BreadcrumbGet(Crumb::Scene);
        torn += value != a && value != b;
    }
    stop.store(true);
    writer.join();
    EXPECT_EQ(torn, 0);
}

} // namespace
