#pragma once

#include <cstddef>

void PortAbsBankFormatBalance(char *buf, std::size_t len);

bool PortAbsBankShouldShowHud();

void PortAbsBankDrawDungeonHud(float statusbar_rate);

bool PortAbsBankShouldShowMenuBalance();

void PortAbsBankDrawMenuBalance(float x, float y);

void PortAbsBankDrawItemMenuOverlays();
