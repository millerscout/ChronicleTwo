#include "town_draw_distance.hpp"

#ifdef PORT

#include "editctrl_port.hpp"
#include "editloop.hpp"
#include "mainloop.hpp"
#include "mainloop_port.hpp"
#include "platform/config.hpp"
#include "scene.hpp"

namespace {

constexpr float kExtendedFarMultiplier = 1.5f;

} // namespace

ConfigTownDrawDistance PortTownDrawDistanceMode() { return ConfigGet().town_draw_distance; }

bool PortTownDrawDistanceApplies() {
    if (PortTownDrawDistanceMode() != ConfigTownDrawDistance::Extended) {
        return false;
    }
    if (GetNowLoopNo() != LOOP_EDIT) {
        return false;
    }
    if (IsEditMode()) {
        return false;
    }
    if (PortTownLoopMode != EDIT_LOOP_WALK || PortTownControlMode != EDIT_CONTROL_PLAYER || PortTownLockChara != 0 ||
        PortTownPause != 0) {
        return false;
    }
    const CScene *scene = GetMainScene();
    return scene != nullptr && scene->event_run == 0;
}

float PortTownEffectiveFarDist(float retail_far) {
    if (retail_far <= 0.0f || !PortTownDrawDistanceApplies()) {
        return retail_far;
    }
    return retail_far * kExtendedFarMultiplier;
}

#endif
