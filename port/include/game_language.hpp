#pragma once

#include <string_view>

// PAL language ids (LANG_ENGLISH..LANG_SPANISH). Zero means unset or invalid.
int ConfigParseGameLanguage(std::string_view name);
const char *ConfigGameLanguageString(int language);

// Configured game.language, or zero when absent.
int PortConfiguredGameLanguage();

void PortPersistGameLanguage(int language);
void PortApplyGameLanguage(int language);
