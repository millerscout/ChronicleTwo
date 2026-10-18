#include "common.h"

#include "options/apply.hpp"

#include "game_language.hpp"
#include "i18n/i18n.hpp"
#include "menuop.hpp"
#include "options/config_bool.hpp"
#include "platform/display.hpp"
#include "title.hpp"

extern CMenuOption *PortMenuOption asm("MenuOption_Current");

namespace options {

ApplyResult ApplyConfig(const Config &edited, std::string_view row_key) {
    Config config = edited;
    if (row_key == "game.debug_mode") {
        port_settings::WriteConfigBool(config, row_key, config.debug_mode);
    }
    const int  language_before = ConfigGet().game_language;
    const bool saved = DisplayApply([&] { return ConfigChange(config); });
    if ((row_key == "game.language" || row_key == "game.ui_language") && config.game_language != language_before) {
        PortApplyGameLanguage(config.game_language);
        TitleLangSelSetLanguage(config.game_language);
        if (PortMenuOption != nullptr) {
            PortMenuOption->UpdateOptionForm();
        }
    }
    return {true, saved};
}

ApplyResult ApplyRowStep(const Row &row, int direction) {
    Config config = ConfigGet();
    if (!StepRow(row, config, direction)) {
        return {};
    }
    return ApplyConfig(config, row.key);
}

ApplyResult ApplyRowSelect(const Row &row, int index) {
    if (!row.select || !row.index) {
        return {};
    }
    Config config = ConfigGet();
    if (row.index(config) == index) {
        return {};
    }
    row.select(config, index);
    if (config == ConfigGet()) {
        return {};
    }
    return ApplyConfig(config, row.key);
}

ApplyResult ApplyBinding(std::string_view action, std::string_view key) {
    Config config = ConfigGet();
    if (!SetBinding(config, action, key)) {
        return {};
    }
    return ApplyConfig(config, "");
}

ApplyResult ResetRowBinding(std::string_view action) {
    Config config = ConfigGet();
    ResetBinding(config, action);
    if (config == ConfigGet()) {
        return {};
    }
    return ApplyConfig(config, "");
}

ApplyResult AssignPadButton(std::uint16_t ps2_button, std::uint16_t physical_button) {
    Config config = ConfigGet();
    PadRemapAssign(config.pad_remap, ps2_button, physical_button);
    if (config == ConfigGet()) {
        return {};
    }
    return ApplyConfig(config, "input.pad_remap");
}

ApplyResult ApplyUiLanguage(std::string_view tag) {
    Config config = ConfigGet();
    if (tag == i18n::kAuto) {
        config.ui_language = std::string(tag);
    } else if (const auto *catalog = i18n::FindCatalog(tag)) {
        config.ui_language = catalog->tag;
        config.game_language = catalog->game_language;
    } else {
        return {};
    }
    if (config == ConfigGet()) {
        return {};
    }
    return ApplyConfig(config, "game.ui_language");
}

ApplyResult ResetPadRemap() {
    Config config = ConfigGet();
    config.pad_remap = PadRemapIdentity();
    if (config == ConfigGet()) {
        return {};
    }
    return ApplyConfig(config, "input.pad_remap");
}

} // namespace options
