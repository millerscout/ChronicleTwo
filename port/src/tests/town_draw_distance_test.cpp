#include <gtest/gtest.h>

#include "editctrl_port.hpp"
#include "editloop.hpp"
#include "mainloop.hpp"
#include "mainloop_port.hpp"
#include "platform/config.hpp"
#include "scene.hpp"
#include "town_draw_distance.hpp"

namespace {

class TownDrawDistanceTest : public testing::Test {
protected:
    void SetUp() override {
        saved_config_ = ConfigGet();
        saved_loop_ = PortLoopNo;
        saved_town_mode_ = PortTownLoopMode;
        saved_control_ = PortTownControlMode;
        saved_lock_ = PortTownLockChara;
        saved_pause_ = PortTownPause;
        if (CScene *scene = GetMainScene()) {
            saved_event_run_ = scene->event_run;
            scene->event_run = 0;
        }

        PortLoopNo = LOOP_EDIT;
        PortTownLoopMode = EDIT_LOOP_WALK;
        PortTownControlMode = EDIT_CONTROL_PLAYER;
        PortTownLockChara = 0;
        PortTownPause = 0;
    }

    void TearDown() override {
        ConfigChange(saved_config_);
        PortLoopNo = saved_loop_;
        PortTownLoopMode = saved_town_mode_;
        PortTownControlMode = saved_control_;
        PortTownLockChara = saved_lock_;
        PortTownPause = saved_pause_;
        if (CScene *scene = GetMainScene()) {
            scene->event_run = saved_event_run_;
        }
    }

    void SetExtended() {
        Config config = ConfigGet();
        config.town_draw_distance = ConfigTownDrawDistance::Extended;
        ConfigChange(config);
    }

    void SetRetail() {
        Config config = ConfigGet();
        config.town_draw_distance = ConfigTownDrawDistance::Retail;
        ConfigChange(config);
    }

private:
    Config saved_config_;
    int    saved_loop_;
    int    saved_town_mode_;
    int    saved_control_;
    int    saved_lock_;
    int    saved_pause_;
    int    saved_event_run_;
};

} // namespace

TEST_F(TownDrawDistanceTest, ExtendedMultipliesPositiveFarDistanceInTownWalk) {
    SetExtended();
    EXPECT_TRUE(PortTownDrawDistanceApplies());
    EXPECT_FLOAT_EQ(PortTownEffectiveFarDist(800.0f), 1200.0f);
}

TEST_F(TownDrawDistanceTest, RetailLeavesFarDistanceUnchanged) {
    SetRetail();
    EXPECT_FALSE(PortTownDrawDistanceApplies());
    EXPECT_FLOAT_EQ(PortTownEffectiveFarDist(800.0f), 800.0f);
}

TEST_F(TownDrawDistanceTest, NonPositiveFarDistanceIsUnchanged) {
    SetExtended();
    EXPECT_FLOAT_EQ(PortTownEffectiveFarDist(-1.0f), -1.0f);
    EXPECT_FLOAT_EQ(PortTownEffectiveFarDist(0.0f), 0.0f);
}

TEST_F(TownDrawDistanceTest, VillagerDistanceScenario) {
    SetExtended();
    const float effective = PortTownEffectiveFarDist(800.0f);
    EXPECT_GT(900.0f, 800.0f);
    EXPECT_LT(900.0f, effective);
    EXPECT_GT(1000.0f, 800.0f);
    EXPECT_LT(1000.0f, effective);
}

TEST_F(TownDrawDistanceTest, GatingOffInEditMode) {
    SetExtended();
    // IsEditMode is global; only assert retail passthrough when applies is false due to loop.
    PortLoopNo = LOOP_DUNGEON;
    EXPECT_FALSE(PortTownDrawDistanceApplies());
    EXPECT_FLOAT_EQ(PortTownEffectiveFarDist(800.0f), 800.0f);
}

TEST_F(TownDrawDistanceTest, GatingOffDuringTownPause) {
    SetExtended();
    PortTownPause = 1;
    EXPECT_FALSE(PortTownDrawDistanceApplies());
    EXPECT_FLOAT_EQ(PortTownEffectiveFarDist(800.0f), 800.0f);
}
