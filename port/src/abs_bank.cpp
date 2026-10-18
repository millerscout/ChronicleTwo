#include "abs_bank.hpp"

#include "gamedata.hpp"
#include "platform/config.hpp"
#include "save_extensions_mod.hpp"

namespace {

constexpr const char *kModId = "chronicle_two.abs_bank";
constexpr const char *kBankedAbsKey = "banked_abs";

float ReadBanked(const nlohmann::json &mod) {
    if (!mod.contains(kBankedAbsKey) || !mod[kBankedAbsKey].is_number()) {
        return 0.0f;
    }
    return mod[kBankedAbsKey].get<float>();
}

float WeaponAbsRoom(const CGameDataUsed *weapon) {
    if (weapon == nullptr || weapon->used_type != USED_ITEM_TYPE_WEAPON) {
        return 0.0f;
    }
    const float max = weapon->data.weapon.abs.max;
    const float now = weapon->data.weapon.abs.now;
    if (max <= now) {
        return 0.0f;
    }
    return max - now;
}

float g_apply_amount = 0.0f;

} // namespace

bool AbsBankEnabled() { return ConfigGet().abs_bank; }

float AbsBankBalance() {
    if (!SaveExtensionsActive()) {
        return 0.0f;
    }
    const nlohmann::json &mods = SaveExtensionsMods();
    if (!mods.contains(kModId) || !mods[kModId].is_object()) {
        return 0.0f;
    }
    return ReadBanked(mods[kModId]);
}

void AbsBankCredit(float amount) {
    if (amount <= 0.0f) {
        return;
    }
    nlohmann::json *mod = SaveExtensionsMod(kModId);
    if (mod == nullptr) {
        return;
    }
    (*mod)[kBankedAbsKey] = ReadBanked(*mod) + amount;
}

bool AbsBankDebit(float amount) {
    if (amount <= 0.0f) {
        return true;
    }
    nlohmann::json *mod = SaveExtensionsMod(kModId);
    if (mod == nullptr) {
        return false;
    }
    const float balance = ReadBanked(*mod);
    if (balance < amount) {
        return false;
    }
    (*mod)[kBankedAbsKey] = balance - amount;
    return true;
}

float AbsBankApplyAmount() { return g_apply_amount; }

void AbsBankSetApplyAmount(float amount) { g_apply_amount = amount < 0.0f ? 0.0f : amount; }

void AbsBankResetApplyAmount() { g_apply_amount = 0.0f; }

int AbsBankApplyLevelUps(CGameDataUsed *weapon) {
    if (weapon == nullptr || weapon->item_no <= 0) {
        return 0;
    }
    int count = 0;
    for (int guard = 0; guard < 20 && weapon->IsLevelUp() != 0; ++guard) {
        weapon->LevelUp();
        ++count;
    }
    return count;
}

float AbsBankWeaponRefundFloor(const CGameDataUsed *weapon) {
    if (weapon == nullptr || weapon->used_type != USED_ITEM_TYPE_WEAPON) {
        return 0.0f;
    }
    // Retail resets abs.now to 0 on level-up; level never decreases when refunding.
    return 0.0f;
}

float AbsBankTransferToWeapon(CGameDataUsed *weapon, float amount) {
    if (weapon == nullptr || weapon->used_type != USED_ITEM_TYPE_WEAPON || amount <= 0.0f) {
        return 0.0f;
    }
    const float room = WeaponAbsRoom(weapon);
    const float balance = AbsBankBalance();
    float         use = amount;
    if (use > room) {
        use = room;
    }
    if (use > balance) {
        use = balance;
    }
    if (use <= 0.0f || !AbsBankDebit(use)) {
        return 0.0f;
    }
    weapon->data.weapon.abs.now += use;
    AbsBankApplyLevelUps(weapon);
    return use;
}

float AbsBankTransferFromWeapon(CGameDataUsed *weapon, float amount) {
    if (weapon == nullptr || weapon->used_type != USED_ITEM_TYPE_WEAPON || amount <= 0.0f) {
        return 0.0f;
    }
    const float floor = AbsBankWeaponRefundFloor(weapon);
    float         available = weapon->data.weapon.abs.now - floor;
    if (available <= 0.0f) {
        return 0.0f;
    }
    float refund = amount;
    if (refund > available) {
        refund = available;
    }
    weapon->data.weapon.abs.now -= refund;
    AbsBankCredit(refund);
    return refund;
}

bool AbsBankApplyToWeapon(CGameDataUsed *weapon) {
    const float use = AbsBankTransferToWeapon(weapon, g_apply_amount);
    if (use <= 0.0f) {
        return false;
    }
    g_apply_amount -= use;
    if (g_apply_amount < 0.0f) {
        g_apply_amount = 0.0f;
    }
    return true;
}

bool AbsBankRouteOrbExperience(float amount) {
    if (!AbsBankEnabled() || amount <= 0.0f) {
        return false;
    }
    AbsBankCredit(amount);
    return true;
}
