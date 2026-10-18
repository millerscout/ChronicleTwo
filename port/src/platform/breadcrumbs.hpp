#pragma once

#include <string>
#include <string_view>

// What the game is doing, kept for the crash report and the session log. Setting a crumb neither allocates nor
// locks, so it can sit on per-frame paths; an unchanged value returns early.
enum class Crumb : int {
    Mode,  // the main loop's mode, with its sub-mode
    Scene, // the town map or scene being shown or loaded
    Menu,  // the open menu, empty when none
    Floor, // the dungeon and floor
    Asset, // the last file the game loaded
    Count,
};

inline constexpr int kCrumbValueSize = 64;

void BreadcrumbSet(Crumb crumb, std::string_view value);
// printf-style; the value is cut at kCrumbValueSize - 1 bytes.
void BreadcrumbSetf(Crumb crumb, const char *format, ...) __attribute__((format(printf, 2, 3)));
std::string      BreadcrumbGet(Crumb crumb);
std::string_view BreadcrumbName(Crumb crumb);
void             BreadcrumbsClear();

// Async-signal-safe: one "name: value" line per crumb.
void BreadcrumbsWrite(int fd);
