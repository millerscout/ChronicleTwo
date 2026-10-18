#include <gtest/gtest.h>

#include "abs_bank_hud.hpp"
#include "dng_main.hpp"
#include "dng_status.hpp"
#include "mainloop_port.hpp"
#include "platform/config.hpp"

TEST(AbsBankHud, FormatBalance) {
    char text[64];
    PortAbsBankFormatBalance(text, sizeof text);
    EXPECT_STREQ(text, "Banked ABS: 0");
}

TEST(AbsBankHud, ShouldShowHudRespectsBankingToggle) {
    Config prior = ConfigGet();
    Config config = prior;
    config.abs_bank = false;
    ConfigChange(config);
    EXPECT_FALSE(PortAbsBankShouldShowHud());
    EXPECT_FALSE(PortAbsBankShouldShowMenuBalance());

    config.abs_bank = true;
    ConfigChange(config);
    EXPECT_TRUE(PortAbsBankShouldShowMenuBalance());
    EXPECT_FALSE(PortAbsBankShouldShowHud());

    PortLoopNo = LOOP_DUNGEON;
    DngStatus.mode = DNG_STATUS_FIELD;
    EXPECT_FALSE(PortAbsBankShouldShowHud());

    ConfigChange(prior);
    PortLoopNo = LOOP_TITLE;
}
