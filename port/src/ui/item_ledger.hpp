#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

class CGameDataUsed;
class CUserDataManager;

// The hub's Item screen, as plain values. BuildLedger fills it from a save through an ItemSource, so the screen
// never calls into the game and tests can supply their own source.
namespace ui {

enum class ItemTab { Equipped, Weapons, Clothing, Ridepod, Attachments, Items, Materials, Fishing, Key, Count };

struct WeaponDetail {
    int                 level = 0;
    int                 attack = 0;
    int                 durability = 0;
    int                 durability_max = 0;
    int                 abs = 0;
    int                 abs_max = 0;
    int                 synthesis = 0;
    std::array<int, 8>  attributes{};
};

struct LedgerEntry {
    bool                        heading = false;
    std::string                 name;
    int                         count = 0;
    bool                        stacks = false;
    bool                        overflow = false;
    std::string                 slot;  // an equipment slot's key, empty for a bag entry
    std::string                 owner; // who wears it, empty for a bag entry
    int                         item_no = 0;
    std::string                 description;
    std::optional<WeaponDetail> weapon;
};

struct LedgerTab {
    std::vector<LedgerEntry> entries;
};

struct ItemLedgerView {
    bool                                has_save = false;
    int                                 gilda = 0;
    std::array<LedgerTab, static_cast<int>(ItemTab::Count)> tabs{};
};

// Which tab an item type belongs to. Empty for no item and for a type the tables do not use.
[[nodiscard]] std::optional<ItemTab> TabForItemType(int type);

class ItemSource {
public:
    virtual ~ItemSource() = default;

    [[nodiscard]] virtual int Type(int item_no) const = 0;
    [[nodiscard]] virtual std::string Name(const CGameDataUsed &item) const = 0;
    [[nodiscard]] virtual bool Stacks(const CGameDataUsed &item) const = 0;
    [[nodiscard]] virtual int Count(const CGameDataUsed &item) const = 0;
    [[nodiscard]] virtual std::string Description(const CGameDataUsed &item) const = 0;
    [[nodiscard]] virtual std::optional<WeaponDetail> Weapon(const CGameDataUsed &item) const = 0;
};

// user null is a ledger with no save. Building reads the save and does not change it.
// Reads the save through its accessors, which are not const. It does not change the save.
[[nodiscard]] ItemLedgerView BuildLedger(CUserDataManager *user, const ItemSource &source);

// The save's own item data: names, descriptions and weapon stats as the game's item menu shows them.
class GameItemSource : public ItemSource {
public:
    [[nodiscard]] int Type(int item_no) const override;
    [[nodiscard]] std::string Name(const CGameDataUsed &item) const override;
    [[nodiscard]] bool Stacks(const CGameDataUsed &item) const override;
    [[nodiscard]] int Count(const CGameDataUsed &item) const override;
    [[nodiscard]] std::string Description(const CGameDataUsed &item) const override;
    [[nodiscard]] std::optional<WeaponDetail> Weapon(const CGameDataUsed &item) const override;
};

// The save the Item screen reads: the dungeon's copy inside a dungeon, else the town save. Null with no save.
[[nodiscard]] CUserDataManager *ItemLedgerUser();

} // namespace ui
