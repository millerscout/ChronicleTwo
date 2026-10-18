#include "common.h"

#include "abs_bank.hpp"
#include "abs_bank_hud.hpp"

#include "gamepad.hpp"
#include "gamedata.hpp"
#include "mainloop.hpp"
#include "menucls1.hpp"
#include "menudraw.hpp"
#include "menucommon.hpp"
#include "menusys.hpp"

CMenuItemInfo &ItemMenu() { return class_menu_item_info; }

namespace {

constexpr float kShoulderTransferStep = 4.0f;

bool ShoulderHeld(int mask) {
    // Retail menu keys use pad 0 (On/Down); port shoulder reads for gameplay often use pad 1 (On2).
    return GamePad__2.On(mask) || GamePad__2.On2(mask);
}

bool WeaponPanelBankingActive() {
    if (!AbsBankEnabled()) {
        return false;
    }
    CMenuItemInfo &menu = ItemMenu();
    if (menu.sub_menu != MENU_ITEM_SUB_MENU_NONE || menu.mode != MENU_ASK_MODE_NONE) {
        return false;
    }
    if (menu.ask_para.ask_mode != MENU_ITEM_EXTEND_NONE) {
        return false;
    }
    if (menu.view_mode != MENU_ITEM_VIEW_WEAPON) {
        return false;
    }
    if (menu.key_arg_no == ITEM_MENU_PANEL_WEAPON_STATS) {
        return false;
    }
    // Main inspect panel, or bag board on the weapon page (gauge still shows view_weapon).
    if (menu.key_arg_no != ITEM_MENU_PANEL_WEAPON && menu.key_arg_no != ITEM_MENU_PANEL_BAG) {
        return false;
    }
    CGameDataUsed *weapon = menu.view_weapon;
    return weapon != nullptr && weapon->used_type == USED_ITEM_TYPE_WEAPON;
}

void PlayWeaponLevelUpEffect() { MenuSePlay(SYSTEM_SE_PARAM_UP); }

void PollWeaponPanelShoulderTransfer() {
    if (!WeaponPanelBankingActive()) {
        return;
    }
    CGameDataUsed *weapon = ItemMenu().view_weapon;
    if (ShoulderHeld(PAD_R2)) {
        const int levels_before = weapon->data.weapon.level;
        if (AbsBankTransferToWeapon(weapon, kShoulderTransferStep) > 0.0f) {
            if (weapon->data.weapon.level > levels_before) {
                PlayWeaponLevelUpEffect();
            }
        }
    }
    if (ShoulderHeld(PAD_L2)) {
        AbsBankTransferFromWeapon(weapon, kShoulderTransferStep);
    }
}

void DrawItemMenuBankBanner() {
    if (!PortAbsBankShouldShowMenuBalance()) {
        return;
    }
    CMenuItemInfo &menu = ItemMenu();
    if (menu.sub_menu != MENU_ITEM_SUB_MENU_NONE) {
        return;
    }
    PortAbsBankDrawMenuBalance(300.0f, 48.0f);
}

void DrawWeaponPanelBankHint() {
    if (!WeaponPanelBankingActive()) {
        return;
    }
    CMenuFont  menu_font;
    CMenuFont *font = &menu_font;
    DrawMenuFillBox(236.0f, 64.0f, 230.0f, 28.0f, 0x80, 0, 0, 0);
    static char hint[] = "R2: add ABS from bank  L2: refund";
    font->SetStr(hint);
    font->SetPos(236, 68);
    font->DrawDirect(font->str, font->pos_x, font->pos_y);
}

} // namespace

void PortAbsBankDrawItemMenuOverlays() {
    DrawItemMenuBankBanner();
    DrawWeaponPanelBankHint();
}

PORT_OVERRIDE void CMenuItemInfo::KeyStepLocal(int select_key, int push_button, int flag) {
    PollWeaponPanelShoulderTransfer();
    PortRetailMenuItemInfoKeyStepLocal(select_key, push_button, flag);
}
