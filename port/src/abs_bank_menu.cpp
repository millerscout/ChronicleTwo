#include "common.h"

#include "abs_bank.hpp"
#include "abs_bank_hud.hpp"

#include "gamedata.hpp"
#include "menucls1.hpp"
#include "menucommon.hpp"
#include "menumain.hpp"
#include "menudraw.hpp"
#include "menusys.hpp"
#include "snd_mngr.hpp"

CMenuItemInfo &ItemMenu() { return class_menu_item_info; }


constexpr float kApplyAnalogScale = 1.0f;

void ClampApplyAmount(CGameDataUsed *weapon) {
    const float room = weapon->data.weapon.abs.max - weapon->data.weapon.abs.now;
    float       cap = AbsBankBalance();
    if (room < cap) {
        cap = room;
    }
    if (cap < 0.0f) {
        cap = 0.0f;
    }
    float amount = AbsBankApplyAmount();
    if (amount > cap) {
        amount = cap;
    }
    if (amount < 0.0f) {
        amount = 0.0f;
    }
    AbsBankSetApplyAmount(amount);
}

bool WeaponStatsBankingActive() {
    if (!AbsBankEnabled()) {
        return false;
    }
    CMenuItemInfo &menu = ItemMenu();
    if (menu.key_arg_no != ITEM_MENU_PANEL_WEAPON_STATS) {
        return false;
    }
    CGameDataUsed *weapon = menu.view_weapon;
    return weapon != nullptr && weapon->used_type == USED_ITEM_TYPE_WEAPON;
}

void DrawItemMenuBankBanner() {
    if (!PortAbsBankShouldShowMenuBalance()) {
        return;
    }
    CMenuItemInfo &menu = ItemMenu();
    if (menu.sub_menu != MENU_ITEM_SUB_MENU_NONE) {
        return;
    }
    // Beside the bag gilda readout (top of the item board).
    PortAbsBankDrawMenuBalance(300.0f, 48.0f);
}

void DrawStatsSpendOverlay() {
    if (!WeaponStatsBankingActive()) {
        return;
    }
    CMenuFont  menu_font;
    CMenuFont *font = &menu_font;
    char       bank_line[0x80];
    char       text[0x100];
    PortAbsBankFormatBalance(bank_line, sizeof bank_line);
    std::sprintf(text, "%s  Apply: %d", bank_line, static_cast<int>(AbsBankApplyAmount()));
    DrawMenuFillBox(236.0f, 64.0f, 230.0f, 36.0f, 0x80, 0, 0, 0);
    font->SetStr(text);
    font->SetPos(236, 68);
    font->DrawDirect(font->str, font->pos_x, font->pos_y);
    font->SetStr("Analog: amount  {FD06} Apply");
    font->SetPos(236, 82);
    font->DrawDirect(font->str, font->pos_x, font->pos_y);
}

void PollWeaponStatsAnalog() {
    if (!WeaponStatsBankingActive()) {
        return;
    }
    CGameDataUsed *weapon = ItemMenu().view_weapon;
    float          analog[2] = {0.0f, 0.0f};
    MenuCommonInfo->CheckAnalogKey(0, analog);
    if (analog[0] == 0.0f) {
        return;
    }
    AbsBankSetApplyAmount(AbsBankApplyAmount() + analog[0] * kApplyAnalogScale);
    ClampApplyAmount(weapon);
}

bool HandleWeaponStatsInput(int trigger) {
    if (!WeaponStatsBankingActive()) {
        return false;
    }
    CGameDataUsed *weapon = ItemMenu().view_weapon;
    if (trigger == 0) {
        return false;
    }
    if (trigger & MENU_PUSH_BUTTON_CANCEL) {
        AbsBankResetApplyAmount();
        return false;
    }
    if (trigger & MENU_PUSH_BUTTON_DECIDE) {
        if (AbsBankApplyToWeapon(weapon)) {
            MenuSePlay(SYSTEM_SE_DECIDE);
        } else {
            MenuSePlay(SYSTEM_SE_DENIED);
        }
        ClampApplyAmount(weapon);
        return true;
    }
    if (trigger & (MENU_PUSH_BUTTON_TRIANGLE | MENU_PUSH_BUTTON_SQUARE)) {
        return true;
    }
    return false;
}

void PortAbsBankDrawItemMenuOverlays() {
    DrawItemMenuBankBanner();
    DrawStatsSpendOverlay();
}

PORT_OVERRIDE void CMenuItemInfo::KeyStepLocal(int select_key, int push_button, int flag) {
    PollWeaponStatsAnalog();
    PortRetailMenuItemInfoKeyStepLocal(select_key, push_button, flag);
}

PORT_OVERRIDE int CMenuItemInfo::PushKey(int pad, int trigger) {
    if (HandleWeaponStatsInput(trigger)) {
        return 0;
    }
    return PortRetailMenuItemInfoPushKey(pad, trigger);
}
