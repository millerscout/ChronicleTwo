#include <gtest/gtest.h>

#include "dungeon_ceremony_qol.hpp"
#include "dng_main.hpp"
#include "mainloop_port.hpp"
#include "nd_meswin.hpp"
#include "platform/config.hpp"

TEST(DungeonCeremonyQoL, CompleteRevealFinishesTypewriter) {
    ClsMes mes;
    mes.Init();
    mes.char_num = 12;
    mes.reveal_num = 3;
    mes.reveal = 3.5f;
    mes.draw_speed = 0.6f;
    mes.wait = 4;
    mes.CompleteReveal();
    EXPECT_EQ(mes.reveal_num, mes.char_num);
    EXPECT_EQ(mes.wait, 0);
    EXPECT_FLOAT_EQ(mes.reveal, static_cast<float>(mes.char_num));
}

TEST(DungeonCeremonyQoL, ManualAcceleratesMessageWindowOpenFade) {
    Config config;
    config.dungeon_ceremony_qol = ConfigDungeonCeremonyQoL::Manual;
    ConfigChange(config);
    PortLoopNo = LOOP_DUNGEON;
    DngStatus.mode = DNG_STATUS_EVENT;
    PortDungeonCeremonyQoLOnEventStart(DNG_EVENT_TREASURE_BOX);

    ClsMes mes;
    mes.open = 1;
    mes.fade = 0.0f;
    mes.fade_speed = 0.1f;
    PortDungeonCeremonyQoLAccelerateMessageOpen(&mes);
    EXPECT_FLOAT_EQ(mes.fade, 0.1f);

    PortDungeonCeremonyQoLOnEventEnd();
    PortLoopNo = LOOP_TITLE;
}

TEST(DungeonCeremonyQoL, AutoSnapsMessageWindowOpenFade) {
    Config config;
    config.dungeon_ceremony_qol = ConfigDungeonCeremonyQoL::Auto;
    ConfigChange(config);
    PortLoopNo = LOOP_DUNGEON;
    DngStatus.mode = DNG_STATUS_EVENT;
    PortDungeonCeremonyQoLOnEventStart(DNG_EVENT_TREASURE_BOX);

    ClsMes mes;
    mes.open = 1;
    mes.fade = 0.2f;
    mes.fade_speed = 0.1f;
    PortDungeonCeremonyQoLAccelerateMessageOpen(&mes);
    EXPECT_FLOAT_EQ(mes.fade, 1.0f);

    PortDungeonCeremonyQoLOnEventEnd();
    PortLoopNo = LOOP_TITLE;
}

TEST(DungeonCeremonyQoL, GatingRespectsModeAndEvent) {
    Config config;
    config.dungeon_ceremony_qol = ConfigDungeonCeremonyQoL::Manual;
    ConfigChange(config);

    PortLoopNo = LOOP_DUNGEON;
    DngStatus.mode = DNG_STATUS_EVENT;
    PortDungeonCeremonyQoLOnEventStart(DNG_EVENT_FLOOR_CLEAR);
    EXPECT_TRUE(PortIsDungeonCeremonyQoLActive());

    PortDungeonCeremonyQoLOnEventStart(DNG_EVENT_LEVEL_UP_GUIDE);
    EXPECT_FALSE(PortIsDungeonCeremonyQoLActive());

    config.dungeon_ceremony_qol = ConfigDungeonCeremonyQoL::Off;
    ConfigChange(config);
    PortDungeonCeremonyQoLOnEventStart(DNG_EVENT_TREASURE_BOX);
    EXPECT_FALSE(PortIsDungeonCeremonyQoLActive());

    PortDungeonCeremonyQoLOnEventEnd();
    PortLoopNo = LOOP_TITLE;
}
