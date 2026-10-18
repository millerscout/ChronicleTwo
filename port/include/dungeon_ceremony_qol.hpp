#pragma once

#include <cstdint>

class ClsMes;

enum class ConfigDungeonCeremonyQoL : std::uint8_t {
    Off    = 0,
    Manual = 1,
    Auto   = 2,
};

#ifdef PORT

ConfigDungeonCeremonyQoL PortDungeonCeremonyQoLMode();

bool PortIsDungeonCeremonyQoLActive();

void PortDungeonCeremonyQoLOnEventStart(int event_no);
void PortDungeonCeremonyQoLOnEventEnd();

void PortDungeonCeremonyQoLEventFrame();

void PortDungeonCeremonyQoLAccelerateMessageOpen(ClsMes *mes);

bool PortDungeonCeremonyQoLShouldDoubleResume();

float PortDungeonCeremonyQoLMotionStepScale(float step);

#else

inline ConfigDungeonCeremonyQoL PortDungeonCeremonyQoLMode() { return ConfigDungeonCeremonyQoL::Off; }
inline bool                     PortIsDungeonCeremonyQoLActive() { return false; }
inline void                     PortDungeonCeremonyQoLOnEventStart(int) {}
inline void                     PortDungeonCeremonyQoLOnEventEnd() {}
inline void                     PortDungeonCeremonyQoLEventFrame() {}
inline void                     PortDungeonCeremonyQoLAccelerateMessageOpen(ClsMes *) {}
inline bool                     PortDungeonCeremonyQoLShouldDoubleResume() { return false; }
inline float                    PortDungeonCeremonyQoLMotionStepScale(float step) { return step; }

#endif
