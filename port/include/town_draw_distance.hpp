#pragma once

#include <cstdint>

enum class ConfigTownDrawDistance : std::uint8_t {
    Retail   = 0,
    Extended = 1,
};

#ifdef PORT

ConfigTownDrawDistance PortTownDrawDistanceMode();

bool PortTownDrawDistanceApplies();

float PortTownEffectiveFarDist(float retail_far);

#else

inline ConfigTownDrawDistance PortTownDrawDistanceMode() { return ConfigTownDrawDistance::Retail; }
inline bool                     PortTownDrawDistanceApplies() { return false; }
inline float                    PortTownEffectiveFarDist(float retail_far) { return retail_far; }

#endif
