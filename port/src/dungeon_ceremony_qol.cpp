#include "dungeon_ceremony_qol.hpp"

#ifdef PORT

#include "dng_main.hpp"
#include "event.hpp"
#include "mainloop_port.hpp"
#include "nd_meswin.hpp"
#include "padcontrol.hpp"
#include "platform/config.hpp"

namespace {

int g_ceremony_event_no = -1;

bool IsCeremonyEventNo(int event_no) {
    switch (event_no) {
        case DNG_EVENT_FLOOR_START:
        case DNG_EVENT_TREASURE_BOX:
        case DNG_EVENT_GEOSTONE:
        case DNG_EVENT_FLOOR_CLEAR:
            return true;
        default:
            return false;
    }
}

bool CeremonyContextActive() {
    if (PortLoopNo != LOOP_DUNGEON || DngStatus.mode != DNG_STATUS_EVENT) {
        return false;
    }
    if (!IsCeremonyEventNo(g_ceremony_event_no)) {
        return false;
    }
    return PortDungeonCeremonyQoLMode() != ConfigDungeonCeremonyQoL::Off;
}

void CompleteRevealIfNeeded(ClsMes *mes) {
    if (mes == nullptr || mes->open == 0) {
        return;
    }
    if (mes->draw_speed <= 0.0f || mes->reveal_num >= mes->char_num) {
        return;
    }
    mes->CompleteReveal();
}

void ProcessEventMessages() {
    const auto mode = PortDungeonCeremonyQoLMode();
    if (!CeremonyContextActive()) {
        return;
    }

    if (mode == ConfigDungeonCeremonyQoL::Auto) {
        CompleteRevealIfNeeded(GetEventMessage(0));
        CompleteRevealIfNeeded(GetEventMessage(1));
        return;
    }

    if (mode == ConfigDungeonCeremonyQoL::Manual) {
        if (!PadCtrl.Btn(PAD_BTN_CONFIRM) && !PadCtrl.Btn(PAD_BTN_EVENT_SKIP_2)) {
            return;
        }
        CompleteRevealIfNeeded(GetEventMessage(0));
        CompleteRevealIfNeeded(GetEventMessage(1));
    }
}

} // namespace

ConfigDungeonCeremonyQoL PortDungeonCeremonyQoLMode() {
    return ConfigGet().dungeon_ceremony_qol;
}

bool PortIsDungeonCeremonyQoLActive() { return CeremonyContextActive(); }

void PortDungeonCeremonyQoLOnEventStart(int event_no) { g_ceremony_event_no = event_no; }

void PortDungeonCeremonyQoLOnEventEnd() { g_ceremony_event_no = -1; }

void PortDungeonCeremonyQoLEventFrame() { ProcessEventMessages(); }

bool PortDungeonCeremonyQoLShouldDoubleResume() { return CeremonyContextActive(); }

float PortDungeonCeremonyQoLMotionStepScale(float step) {
    if (!CeremonyContextActive()) {
        return step;
    }
    return step * 2.0f;
}

#endif
