#pragma once

#include <cstdint>
#include <string_view>

#include "options/rows.hpp"

namespace options {

struct ApplyResult {
    bool changed = false;
    // False when the change could not be written to config.json.
    bool saved = true;
};

// Every menu that edits settings goes through these, so PC SETTINGS and the hub save and apply a row
// the same way (display changes, the debug bool, and the game language switch).
ApplyResult ApplyRowStep(const Row &row, int direction);
ApplyResult ApplyRowSelect(const Row &row, int index);
ApplyResult ApplyBinding(std::string_view action, std::string_view key);
ApplyResult ResetRowBinding(std::string_view action);
// Gives ps2_button to the physical controller button (named by its default bit), swapping with the
// physical button that had it.
ApplyResult AssignPadButton(std::uint16_t ps2_button, std::uint16_t physical_button);
ApplyResult ResetPadRemap();
// Makes tag (auto or a loaded catalog) the UI language and switches the game language to the one the
// catalog names; an unknown tag changes nothing.
ApplyResult ApplyUiLanguage(std::string_view tag);
// Saves an already-edited config whose change came from row_key.
ApplyResult ApplyConfig(const Config &config, std::string_view row_key);

} // namespace options
