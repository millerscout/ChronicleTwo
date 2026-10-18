#include "abs_bank_hud.hpp"

#include "abs_bank.hpp"

#include "actionchara.hpp"
#include "dng_main.hpp"
#include "dng_status.hpp"
#include "mainloop.hpp"
#include "mainloop_port.hpp"
#include "font.hpp"
#include "menudraw.hpp"
#include "photo.hpp"
#include "subgame.hpp"
#include "userdata.hpp"

#include <cstdint>
#include <cstdio>

namespace {

bool DungeonFieldHudActive() {
    if (PortLoopNo != LOOP_DUNGEON || DngStatus.mode != DNG_STATUS_FIELD || DngStatus.eye_view || SubGameRunning() ||
        MainChara__2 == nullptr || BattleAreaScene == nullptr || DngUserData == nullptr) {
        return false;
    }
    const int chara = DngUserData->active_chr_no;
    if (chara < USER_CHARA_MAX || chara > USER_CHARA_ROBO) {
        return false;
    }
    CBattleCharaInfo *info = GetBattleCharaInfo();
    if (info != nullptr && info->GetNowHp_i() <= 0) {
        return false;
    }
    return BattleAreaScene->statusbar_show && !(BattleAreaScene->pause_flag & DNG_PAUSE_PLAYER_CONTROL) &&
           MainChara__2->CheckRunEvent() && !NowTakePhoto();
}

void DrawBalanceLabel(float x, float y, uint8_t alpha) {
    char text[0x80];
    PortAbsBankFormatBalance(text, sizeof text);
    CFont font;
    font.Init();
    font.SetDrawSize(16, 20);
    font.SetClearance(16, 20);
    font.SetFuchi(FUCHI_OUTLINE);
    font.SetColor(0x80, 0x80, 0x80, alpha);
    int w = 0;
    int h = 0;
    font.CalcDrawWH(text, &w, &h);
    DrawMenuFillBox(x, y, static_cast<float>(w + 12), static_cast<float>(h + 6), 0x80, 0, 0, 0);
    font.DrawDirect(text, static_cast<int>(x + 6.0f), static_cast<int>(y + 3.0f));
}

} // namespace

void PortAbsBankFormatBalance(char *buf, std::size_t len) {
    if (buf == nullptr || len == 0) {
        return;
    }
    std::snprintf(buf, len, "Banked ABS: %d", static_cast<int>(AbsBankBalance()));
}

bool PortAbsBankShouldShowHud() {
    return AbsBankEnabled() && DungeonFieldHudActive();
}

void PortAbsBankDrawDungeonHud(float statusbar_rate) {
    if (!PortAbsBankShouldShowHud()) {
        return;
    }
    // Same vertical anchor as the quick loadout panel (port/src/quick_loadout.cpp).
    const int top = static_cast<int>(80.0f * statusbar_rate) - 72 + 66;
    const int left = 52;
    DrawBalanceLabel(static_cast<float>(left), static_cast<float>(top - 22), 0x80);
}

bool PortAbsBankShouldShowMenuBalance() { return AbsBankEnabled(); }

void PortAbsBankDrawMenuBalance(float x, float y) {
    if (!PortAbsBankShouldShowMenuBalance()) {
        return;
    }
    DrawBalanceLabel(x, y, 0x80);
}
