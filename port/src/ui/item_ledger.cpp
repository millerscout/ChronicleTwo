#include "ui/item_ledger.hpp"

#include <array>
#include <span>

#include "dng_main.hpp"
#include "gamedata.hpp"
#include "mainloop.hpp"
#include "mainloop_port.hpp"
#include "menumain.hpp"
#include "nd_meswin.hpp"
#include "platform/legacy_encoding.hpp"
#include "savedata.hpp"
#include "userdata.hpp"

namespace ui {
namespace {

// ItemTab for each ITEM_DATA_TYPE, or -1 for none. Indexed by the type's value.
constexpr std::array<int, 36> kTabs = {
    -1,                                         // ITEM_DATA_NONE
    1, 1, 1, 1,                                 // Max and Monica's weapons
    2, 2, 2, 2, 2, 2,                           // clothing
    3, 3, 3, 3, 3,                              // ridepod
    4, 4, 4, 4,                                 // attachments
    6,                                          // georama materials
    8,                                          // badge box, a key item
    5, 5, 5, 5,                                 // recovery, throwing, powder, amulet
    8, 8,                                       // dungeon keys, story items
    8,                                          // gift box
    7, 7,                                       // aquarium, fish
    -1,                                         // 31 is unused
    7,                                          // lures
    8,                                          // other items
    4,                                          // monster drop attachment
    7,                                          // boiled fish
};

void AddBag(LedgerTab &tab, const CGameDataUsed &item, bool overflow, const ItemSource &source) {
    LedgerEntry entry;
    entry.name = source.Name(item);
    entry.stacks = source.Stacks(item);
    entry.count = source.Count(item);
    entry.overflow = overflow;
    entry.item_no = item.item_no;
    entry.description = source.Description(item);
    entry.weapon = source.Weapon(item);
    tab.entries.push_back(std::move(entry));
}

void AddSlot(LedgerTab &tab, const CGameDataUsed &item, std::string_view slot, std::string_view owner,
             const ItemSource &source) {
    LedgerEntry entry;
    entry.slot = slot;
    entry.owner = owner;
    if (item.item_no > 0) {
        entry.name = source.Name(item);
        entry.item_no = item.item_no;
        entry.description = source.Description(item);
        entry.weapon = source.Weapon(item);
    }
    tab.entries.push_back(std::move(entry));
}

void AddWorn(ItemLedgerView &view, std::string heading, std::string_view owner, const CGameDataUsed *equip,
             std::span<const char *const> slots, const CGameDataUsed *active, int active_count, const ItemSource &source) {
    LedgerTab &tab = view.tabs[static_cast<int>(ItemTab::Equipped)];
    tab.entries.push_back({.heading = true, .name = std::move(heading)});
    for (int slot = 0; slot < static_cast<int>(slots.size()); ++slot) {
        AddSlot(tab, equip[slot], slots[static_cast<std::size_t>(slot)], owner, source);
    }
    for (int slot = 0; slot < active_count; ++slot) {
        AddSlot(tab, active[slot], "active", owner, source);
    }
}

} // namespace

std::optional<ItemTab> TabForItemType(int type) {
    if (type < 0 || static_cast<std::size_t>(type) >= kTabs.size() || kTabs[static_cast<std::size_t>(type)] < 0) {
        return std::nullopt;
    }
    return static_cast<ItemTab>(kTabs[static_cast<std::size_t>(type)]);
}

ItemLedgerView BuildLedger(CUserDataManager *user, const ItemSource &source) {
    ItemLedgerView view;
    if (user == nullptr) {
        return view;
    }
    view.has_save = true;
    view.gilda = user->money;

    const int main_board = user->GetItemBoardMaxNum(ITEM_BOARD_MAIN);
    const int all = user->GetItemBoardMaxNum(ITEM_BOARD_ALL);
    for (int index = 0; index < all; ++index) {
        const CGameDataUsed *item = user->GetUsedDataPtr(index);
        if (item == nullptr || item->item_no <= 0) {
            continue;
        }
        const auto tab = TabForItemType(source.Type(item->item_no));
        if (!tab || *tab == ItemTab::Equipped) {
            continue;
        }
        AddBag(view.tabs[static_cast<int>(*tab)], *item, index >= main_board, source);
    }

    for (const int chara : {USER_CHARA_MAX, USER_CHARA_MONICA}) {
        const CHARA_DATA *data = user->GetCharaDataPtr(chara);
        if (data == nullptr) {
            continue;
        }
        const char *name = chara == USER_CHARA_MAX ? "Max" : "Monica";
        static constexpr std::array<const char *, 5> kEquip = {"melee", "sub", "head", "feet", "clothes"};
        AddWorn(view, name, name, data->equip, kEquip, data->active_item, 3, source);
    }
    static constexpr std::array<const char *, 4> kParts = {"arm", "body", "pack", "leg"};
    AddWorn(view, "Ridepod", "Ridepod", user->robo_data.parts, kParts, nullptr, 0, source);
    return view;
}

namespace {

// GetMsgAddInfo only reads the item; the retail method is not const.
CGameDataUsed &Mutable(const CGameDataUsed &item) {
    return const_cast<CGameDataUsed &>(item);
}

} // namespace

int GameItemSource::Type(int item_no) const {
    return GetItemDataType(item_no);
}

std::string GameItemSource::Name(const CGameDataUsed &item) const {
    const char *name = Mutable(item).GetName(ITEM_NAME_TYPE_PLAIN);
    return name != nullptr ? legacy_encoding::NameToUtf8(name) : std::string{};
}

bool GameItemSource::Stacks(const CGameDataUsed &item) const {
    return Mutable(item).CheckTypeEnableStack() != 0;
}

int GameItemSource::Count(const CGameDataUsed &item) const {
    return Mutable(item).GetNum();
}

std::string GameItemSource::Description(const CGameDataUsed &item) const {
    const int message = GetItemMessageNo(item.item_no, 1);
    if (message < 0) {
        return {};
    }
    ClsMes mes;
    mes.SetBuff(GetMenuMainMessageBuffer());
    const u16 *line = mes.GetTextLineDataTop(message);
    if (line == nullptr) {
        return {};
    }
    char *names[2] = {};
    int   numbers[3] = {};
    Mutable(item).GetMsgAddInfo(names, &names[1], numbers);
    std::array<std::string, 2> decoded;
    legacy_encoding::MessageInserts inserts;
    for (int i = 0; i < 2; ++i) {
        if (names[i] != nullptr) {
            decoded[static_cast<std::size_t>(i)] = legacy_encoding::NameToUtf8(names[i]);
            inserts.names[static_cast<std::size_t>(i)] = decoded[static_cast<std::size_t>(i)];
        }
    }
    inserts.numbers[0] = numbers[0];
    inserts.numbers[1] = numbers[1];
    return legacy_encoding::MessageToUtf8(line, inserts);
}

std::optional<WeaponDetail> GameItemSource::Weapon(const CGameDataUsed &item) const {
    if (item.used_type != USED_ITEM_TYPE_WEAPON) {
        return std::nullopt;
    }
    short param[10] = {};
    const float time = GetSaveData() != nullptr ? GetSaveData()->now_time : 0;
    Mutable(item).GetStatusParam(param, time);
    const WEAPON_USED &weapon = item.data.weapon;
    WeaponDetail detail;
    detail.level = weapon.level;
    detail.attack = param[0];
    detail.durability = static_cast<int>(weapon.whp.now);
    detail.durability_max = static_cast<int>(weapon.whp.max);
    detail.abs = static_cast<int>(weapon.abs.now);
    detail.abs_max = static_cast<int>(weapon.abs.max);
    detail.synthesis = weapon.fusion_point;
    for (int i = 0; i < 8; ++i) {
        detail.attributes[static_cast<std::size_t>(i)] = param[2 + i];
    }
    return detail;
}

CUserDataManager *ItemLedgerUser() {
    if (PortLoopNo == LOOP_DUNGEON && DngUserData != nullptr) {
        return DngUserData;
    }
    return GetUserDataMan();
}

} // namespace ui
