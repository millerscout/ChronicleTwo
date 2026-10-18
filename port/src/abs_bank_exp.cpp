#include "common.h"

#include "abs_bank.hpp"
#include "maintex.hpp"

PORT_OVERRIDE void AddExpWeaponParam(float amount, int weapon_owner, int kind) {
    if (AbsBankRouteOrbExperience(amount)) {
        return;
    }
    PortRetailAddExpWeaponParam(amount, weapon_owner, kind);
}
