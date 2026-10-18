#include <gtest/gtest.h>

#include "menuop.hpp"
#include "options/registry.hpp"
#include "options/retail_system_mes.hpp"

TEST(PortSettingsRegistry, HostBoolsMapToConfigKeysAndRetailRows) {
    const auto entries = port_settings::Descriptors();
    ASSERT_EQ(entries.size(), 11u);

    const auto *debug = port_settings::FindByConfigKey("game.debug_mode");
    const auto *ceremony = port_settings::FindByConfigKey("game.dungeon_ceremony_qol");
    const auto *pickup = port_settings::FindByConfigKey("game.dungeon_pickup_qol");
    const auto *scoop = port_settings::FindByConfigKey("game.scoop_tracker");
    const auto *skip = port_settings::FindByConfigKey("game.skip_intro");
    ASSERT_NE(debug, nullptr);
    ASSERT_NE(skip, nullptr);

    EXPECT_STREQ(debug->id, "debug_mode");
    EXPECT_EQ(debug->retail_row, OPTION_ITEM_UNK_9);
    EXPECT_FALSE(port_settings::HasSurface(debug->surfaces, port_settings::Surface::Retail));
    EXPECT_TRUE(port_settings::HasSurface(debug->surfaces, port_settings::Surface::PcOverlay));

    EXPECT_STREQ(skip->id, "skip_intro");
    EXPECT_EQ(skip->retail_row, OPTION_ITEM_SKIP_INTRO);
    EXPECT_FALSE(port_settings::HasSurface(skip->surfaces, port_settings::Surface::Retail));
    EXPECT_TRUE(port_settings::HasSurface(skip->surfaces, port_settings::Surface::PcOverlay));

    EXPECT_EQ(port_settings::FindByRetailRow(OPTION_ITEM_UNK_9), debug);
    EXPECT_EQ(port_settings::FindByRetailRow(OPTION_ITEM_SKIP_INTRO), skip);
    ASSERT_NE(ceremony, nullptr);
    EXPECT_STREQ(ceremony->id, "dungeon_ceremony_qol");
    EXPECT_EQ(ceremony->retail_row, OPTION_ITEM_DUNGEON_CEREMONY_QOL);
    EXPECT_TRUE(port_settings::HasSurface(ceremony->surfaces, port_settings::Surface::Retail));
    EXPECT_TRUE(port_settings::HasSurface(ceremony->surfaces, port_settings::Surface::PcOverlay));

    ASSERT_NE(pickup, nullptr);
    EXPECT_STREQ(pickup->id, "dungeon_pickup_qol");
    EXPECT_EQ(pickup->retail_row, OPTION_ITEM_DUNGEON_PICKUP_QOL);
    EXPECT_TRUE(port_settings::HasSurface(pickup->surfaces, port_settings::Surface::Retail));
    EXPECT_TRUE(port_settings::HasSurface(pickup->surfaces, port_settings::Surface::PcOverlay));

    ASSERT_NE(scoop, nullptr);
    EXPECT_STREQ(scoop->id, "scoop_tracker");
    EXPECT_EQ(scoop->retail_row, OPTION_ITEM_SCOOP_TRACKER);
    EXPECT_TRUE(port_settings::HasSurface(scoop->surfaces, port_settings::Surface::Retail));
    EXPECT_TRUE(port_settings::HasSurface(scoop->surfaces, port_settings::Surface::PcOverlay));

    const auto *town_draw = port_settings::FindByConfigKey("game.town_draw_distance");
    ASSERT_NE(town_draw, nullptr);
    EXPECT_STREQ(town_draw->id, "town_draw_distance");
    EXPECT_EQ(town_draw->kind, port_settings::Kind::Int);
    EXPECT_EQ(town_draw->retail_row, -1);
    EXPECT_FALSE(port_settings::HasSurface(town_draw->surfaces, port_settings::Surface::Retail));
    EXPECT_TRUE(port_settings::HasSurface(town_draw->surfaces, port_settings::Surface::PcOverlay));

    const auto *preview = port_settings::FindByConfigKey("game.ui_preview");
    ASSERT_NE(preview, nullptr);
    EXPECT_STREQ(preview->id, "ui_preview");
    EXPECT_EQ(preview->kind, port_settings::Kind::Bool);
    EXPECT_EQ(preview->retail_row, -1);
    EXPECT_FALSE(port_settings::HasSurface(preview->surfaces, port_settings::Surface::Retail));
    EXPECT_TRUE(port_settings::HasSurface(preview->surfaces, port_settings::Surface::PcOverlay));

    const auto *theme = port_settings::FindByConfigKey("game.ui_theme");
    ASSERT_NE(theme, nullptr);
    EXPECT_STREQ(theme->id, "ui_theme");
    EXPECT_EQ(theme->kind, port_settings::Kind::Int);
    EXPECT_EQ(theme->retail_row, -1);
    EXPECT_TRUE(port_settings::HasSurface(theme->surfaces, port_settings::Surface::PcOverlay));

    const auto *ui_language = port_settings::FindByConfigKey("game.ui_language");
    ASSERT_NE(ui_language, nullptr);
    EXPECT_STREQ(ui_language->id, "ui_language");
    EXPECT_EQ(ui_language->retail_row, -1);
    EXPECT_FALSE(port_settings::HasSurface(ui_language->surfaces, port_settings::Surface::Retail));
}

TEST(PortSettingsRegistry, PcOverlayPageCountMatchesPagesBuilder) {
    EXPECT_EQ(port_settings::PcOverlayPageCount(), 5);
    EXPECT_EQ(port_settings::PcOverlayPageNames().size(), 5u);
    EXPECT_STREQ(port_settings::PcOverlayPageNames().back(), "Game");
    for (const char *name : port_settings::PcOverlayPageNames()) {
        EXPECT_STRNE(name, "Georama");
    }
}

TEST(PortSettingsRegistry, GeoramaPolyMultiplierRegisteredOnPcOverlay) {
    const auto *entry = port_settings::FindByConfigKey("game.georama_poly_multiplier");
    ASSERT_NE(entry, nullptr);
    EXPECT_STREQ(entry->label, "Georama budget");
    EXPECT_EQ(entry->kind, port_settings::Kind::Int);
    EXPECT_TRUE(port_settings::HasSurface(entry->surfaces, port_settings::Surface::PcOverlay));
}

TEST(PortSettingsRetail, DebugRowVisibleBandMatchesOptionScroll) {
    constexpr int row = OPTION_ITEM_UNK_9;
    for (int top = 0; top < 20; ++top) {
        const bool visible = port_settings::RetailRowVisible(row, top);
        const bool expected = row >= top && row < top + port_settings::kRetailVisibleRows;
        EXPECT_EQ(visible, expected) << "top=" << top;
    }
    EXPECT_TRUE(port_settings::RetailRowVisible(row, 1));
    EXPECT_FALSE(port_settings::RetailRowVisible(row, 0));
    EXPECT_FALSE(port_settings::RetailRowVisible(row, 10));
}

TEST(PortSettingsRetail, SkipIntroRowUsesSyntheticLabelColumn) {
    constexpr int row = OPTION_ITEM_SKIP_INTRO;
    EXPECT_GE(row, port_settings::kRetailVisibleRows);
    const int line_id = PortRetailSyntheticSysMesId(row);
    EXPECT_EQ(line_id, 0x7F00 + row);
}

TEST(PortSettingsRetail, SystemMessageOverrideStartsEmpty) {
    PortRetailClearSystemMessageOverrides();
    const int line_id = PortRetailSyntheticSysMesId(OPTION_ITEM_UNK_9);
    EXPECT_EQ(PortRetailSystemMessageOverride(line_id), nullptr);
    EXPECT_EQ(PortRetailSystemMessageOverride(line_id + 1), nullptr);
}

TEST(PortSettingsRetail, SyntheticSysMesIdMatchesRetailRow) {
    EXPECT_EQ(PortRetailSyntheticSysMesId(OPTION_ITEM_UNK_9), 0x7F00 + OPTION_ITEM_UNK_9);
}
