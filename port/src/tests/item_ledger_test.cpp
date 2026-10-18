#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <ranges>
#include <vector>

#include "gamedata.hpp"
#include "mainloop_port.hpp"
#include "menumain.hpp"
#include "savedata.hpp"
#include "ui/item_ledger.hpp"

namespace {

class FakeSource : public ui::ItemSource {
public:
    int Type(int item_no) const override { return item_no; }
    std::string Name(const CGameDataUsed &item) const override { return "Item " + std::to_string(item.item_no); }
    bool Stacks(const CGameDataUsed &item) const override { return item.item_no == ITEM_DATA_RECOVERY; }
    int Count(const CGameDataUsed &item) const override { return item.item_no == ITEM_DATA_RECOVERY ? 5 : 1; }
    std::string Description(const CGameDataUsed &) const override { return {}; }
    std::optional<ui::WeaponDetail> Weapon(const CGameDataUsed &) const override { return {}; }
};

struct Save {
    std::unique_ptr<CSaveData> data = std::make_unique<CSaveData>();
    CSaveData                 *previous = PortActiveSaveData;

    Save() { PortActiveSaveData = data.get(); }
    ~Save() { PortActiveSaveData = previous; }

    CUserDataManager &User() { return data->user_data; }
};

const ui::LedgerTab &Tab(const ui::ItemLedgerView &view, ui::ItemTab tab) {
    return view.tabs[static_cast<int>(tab)];
}

} // namespace

TEST(ItemLedger, EveryItemTypeMapsToOneTab) {
    EXPECT_FALSE(ui::TabForItemType(ITEM_DATA_NONE).has_value());
    EXPECT_FALSE(ui::TabForItemType(31).has_value());
    for (int type = ITEM_DATA_MAX_MELEE; type <= ITEM_DATA_BOILED; ++type) {
        if (type == 31) {
            continue;
        }
        EXPECT_TRUE(ui::TabForItemType(type).has_value()) << type;
    }
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_MONICA_MELEE), ui::ItemTab::Weapons);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_MAX_HAT), ui::ItemTab::Clothing);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_ROBO_ARM), ui::ItemTab::Ridepod);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_GEM), ui::ItemTab::Attachments);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_MONSTER_DROP), ui::ItemTab::Attachments);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_RECOVERY), ui::ItemTab::Items);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_GEORAMA_MATERIAL), ui::ItemTab::Materials);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_FISH), ui::ItemTab::Fishing);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_BOILED), ui::ItemTab::Fishing);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_DUNGEON_KEY), ui::ItemTab::Key);
    EXPECT_EQ(ui::TabForItemType(ITEM_DATA_BADGE_BOX), ui::ItemTab::Key);
}

TEST(ItemLedger, BagTabsListEachItemOnceInBagOrder) {
    Save save;
    const int types[] = {ITEM_DATA_MONICA_MELEE, ITEM_DATA_MAX_HAT,     ITEM_DATA_ROBO_ARM, ITEM_DATA_GEM,
                         ITEM_DATA_RECOVERY,     ITEM_DATA_GEORAMA_MATERIAL, ITEM_DATA_FISH, ITEM_DATA_DUNGEON_KEY};
    for (int i = 0; i < 8; ++i) {
        save.User().used_data[i].item_no = types[i];
    }
    const ui::ItemLedgerView view = ui::BuildLedger(&save.User(), FakeSource{});
    const ui::ItemTab tabs[] = {ui::ItemTab::Weapons, ui::ItemTab::Clothing, ui::ItemTab::Ridepod, ui::ItemTab::Attachments,
                                ui::ItemTab::Items,   ui::ItemTab::Materials, ui::ItemTab::Fishing, ui::ItemTab::Key};
    for (int i = 0; i < 8; ++i) {
        const auto &entries = Tab(view, tabs[i]).entries;
        ASSERT_EQ(entries.size(), 1u);
        EXPECT_EQ(entries[0].item_no, types[i]);
        EXPECT_FALSE(entries[0].overflow);
    }
    EXPECT_EQ(Tab(view, ui::ItemTab::Items).entries[0].count, 5);
    EXPECT_TRUE(Tab(view, ui::ItemTab::Items).entries[0].stacks);
    EXPECT_EQ(Tab(view, ui::ItemTab::Weapons).entries[0].count, 1);
    EXPECT_FALSE(Tab(view, ui::ItemTab::Weapons).entries[0].stacks);
}

TEST(ItemLedger, EmptyTabAndNoSave) {
    Save save;
    save.User().used_data[0].item_no = ITEM_DATA_RECOVERY;
    const ui::ItemLedgerView view = ui::BuildLedger(&save.User(), FakeSource{});
    EXPECT_TRUE(Tab(view, ui::ItemTab::Fishing).entries.empty());

    EXPECT_FALSE(ui::BuildLedger(nullptr, FakeSource{}).has_save);
}

TEST(ItemLedger, OverflowComesLast) {
    Save save;
    save.User().used_data[0].item_no = ITEM_DATA_RECOVERY;
    const int overflow = save.User().GetItemBoardMaxNum(ITEM_BOARD_MAIN);
    save.User().used_data[overflow].item_no = ITEM_DATA_POWDER;
    const auto &items = Tab(ui::BuildLedger(&save.User(), FakeSource{}), ui::ItemTab::Items).entries;
    ASSERT_EQ(items.size(), 2u);
    EXPECT_FALSE(items[0].overflow);
    EXPECT_TRUE(items[1].overflow);
    EXPECT_EQ(items[1].item_no, ITEM_DATA_POWDER);
}

TEST(ItemLedger, EquippedListsMaxsMeleeSlotAndEmptySlots) {
    Save save;
    save.User().chara_data[USER_CHARA_MAX].equip[CHARA_EQUIP_MELEE].item_no = ITEM_DATA_MAX_MELEE;
    const auto &worn = Tab(ui::BuildLedger(&save.User(), FakeSource{}), ui::ItemTab::Equipped).entries;
    const auto melee = std::ranges::find_if(worn, [](const ui::LedgerEntry &entry) { return entry.slot == "melee" && entry.owner == "Max"; });
    ASSERT_NE(melee, worn.end());
    EXPECT_EQ(melee->name, "Item " + std::to_string(ITEM_DATA_MAX_MELEE));
    EXPECT_EQ(std::count_if(worn.begin(), worn.end(), [](const ui::LedgerEntry &entry) { return entry.slot == "head" && entry.name.empty(); }), 2);
    EXPECT_EQ(std::count_if(worn.begin(), worn.end(), [](const ui::LedgerEntry &entry) { return entry.heading; }), 3);
}

TEST(ItemLedger, DescriptionsDecodeOrSkipWithoutTheArchive) {
    if (GetMenuMainMessageBuffer() == nullptr || GameItemDataManage.GetCommonData(1) == nullptr) {
        GTEST_SKIP() << "item data is not loaded";
    }
    Save save;
    ui::GameItemSource source;
    testing::internal::CaptureStderr();
    int decoded = 0;
    for (int item_no = 1; item_no < 4000; ++item_no) {
        if (GameItemDataManage.GetCommonData(item_no) == nullptr) {
            continue;
        }
        CGameDataUsed item;
        item.Init();
        item.item_no = static_cast<s16>(item_no);
        item.used_type = USED_ITEM_TYPE_ITEM;
        source.Description(item);
        ++decoded;
    }
    const std::string warnings = testing::internal::GetCapturedStderr();
    EXPECT_EQ(warnings.find("message code"), std::string::npos) << warnings;
    EXPECT_GT(decoded, 0);
}

TEST(ItemLedger, BuildingLeavesTheSaveUnchanged) {
    Save save;
    save.User().money = 120;
    save.User().used_data[3].item_no = ITEM_DATA_GEM;
    std::vector<char> before(sizeof(CUserDataManager));
    std::memcpy(before.data(), &save.User(), before.size());
    const ui::ItemLedgerView view = ui::BuildLedger(&save.User(), FakeSource{});
    EXPECT_EQ(std::memcmp(before.data(), &save.User(), before.size()), 0);
    EXPECT_EQ(view.gilda, 120);
}
