#include "options/rows.hpp"

#include <cassert>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <type_traits>
#include <utility>

#include "i18n/i18n.hpp"
#include "mainloop.hpp"
#include "options/registry.hpp"
#include "platform/input.hpp"

namespace options {
namespace {

std::vector<DisplayModeSize> g_sizes{
    {0,    0   },
    {800,  600 },
    {1024, 768 },
    {1280, 720 },
    {1280, 960 },
    {1920, 1080}
};

template <auto Member>
Row Choice(const char *key, const char *label, const char *help, std::vector<std::string> names) {
    using Value = std::remove_cvref_t<decltype(Config{}.*Member)>;
    Row row{key, label, help};
    row.value = [names](const Config &c) { return names.at(static_cast<int>(c.*Member)); };
    row.step = [names](Config &c, int direction) {
        const int n = static_cast<int>(names.size());
        c.*Member = static_cast<Value>((static_cast<int>(c.*Member) + direction + n) % n);
    };
    row.choices = names;
    row.index = [](const Config &c) { return static_cast<int>(c.*Member); };
    row.select = [n = static_cast<int>(names.size())](Config &c, int index) {
        c.*Member = static_cast<Value>(std::clamp(index, 0, n - 1));
    };
    return row;
}

std::string NumberText(double n, double display_scale, const char *suffix) {
    if (n == 0 && std::string_view(suffix) == " FPS") {
        return "Unlimited";
    }
    return std::format("{:.3g}{}", n * display_scale, suffix);
}

template <auto Member>
Row Number(const char *key, const char *label, const char *help, std::vector<double> values,
           double display_scale = 1.0, const char *suffix = "") {
    Row row{key, label, help};
    row.value = [display_scale, suffix](const Config &c) { return NumberText(c.*Member, display_scale, suffix); };
    for (double n : values) {
        row.choices.push_back(NumberText(n, display_scale, suffix));
    }
    row.numeric = true;
    // A hand-edited value sits at the nearest preset.
    row.index = [values](const Config &c) {
        const double current = c.*Member;
        int          best = 0;
        for (int i = 1; i < static_cast<int>(values.size()); ++i) {
            if (std::abs(values[i] - current) < std::abs(values[best] - current)) {
                best = i;
            }
        }
        return best;
    };
    row.select = [values](Config &c, int index) {
        c.*Member = values[std::clamp(index, 0, static_cast<int>(values.size()) - 1)];
    };
    row.step = [values](Config &c, int direction) {
        // Preserve a hand-edited value until this row is changed. Move to the next preset,
        // rather than rounding first and skipping a value on either side of it.
        const double current = c.*Member;
        if (direction > 0) {
            for (double n : values) {
                if (n > current + 0.00001) {
                    c.*Member = n;
                    return;
                }
            }
        } else {
            for (auto i = values.rbegin(); i != values.rend(); ++i) {
                if (*i < current - 0.00001) {
                    c.*Member = *i;
                    return;
                }
            }
        }
    };
    return row;
}

Row GameLanguage() {
    static constexpr const char *kNames[] = {"English", "French", "German", "Italian", "Spanish"};
    Row           row{"game.language", "Language",
                      "PAL game text and voices.\nSkips the boot language screen when set.\n"
                      "Active events may keep the previous language until you reload."};
    row.value = [](const Config &c) {
        const int language =
            c.game_language >= LANG_ENGLISH && c.game_language <= LANG_SPANISH ? c.game_language : LanguageCode;
        return std::string(kNames[language - 1]);
    };
    row.step = [](Config &c, int direction) {
        int language = c.game_language >= LANG_ENGLISH && c.game_language <= LANG_SPANISH ? c.game_language
                                                                                          : LanguageCode;
        language = (language - LANG_ENGLISH + direction + 5) % 5 + LANG_ENGLISH;
        c.game_language = language;
    };
    row.choices.assign(std::begin(kNames), std::end(kNames));
    row.index = [](const Config &c) {
        const int language =
            c.game_language >= LANG_ENGLISH && c.game_language <= LANG_SPANISH ? c.game_language : LanguageCode;
        return language - LANG_ENGLISH;
    };
    row.select = [](Config &c, int index) { c.game_language = LANG_ENGLISH + std::clamp(index, 0, 4); };
    return row;
}

constexpr const char *kUiLanguageAuto = "Auto (game language)";

// Choice 0 is auto; choice i is catalog i - 1.
int UiLanguageIndex(const Config &c) {
    const auto catalogs = i18n::Catalogs();
    for (std::size_t i = 0; i < catalogs.size(); ++i) {
        if (catalogs[i].tag == c.ui_language) {
            return static_cast<int>(i) + 1;
        }
    }
    return 0;
}

void SelectUiLanguage(Config &c, int index) {
    const auto catalogs = i18n::Catalogs();
    index = std::clamp(index, 0, static_cast<int>(catalogs.size()));
    if (index == 0) {
        c.ui_language = std::string(i18n::kAuto);
        return;
    }
    c.ui_language = catalogs[index - 1].tag;
    c.game_language = catalogs[index - 1].game_language;
}

std::vector<std::string> UiLanguageChoices() {
    std::vector<std::string> names{kUiLanguageAuto};
    for (const auto &catalog : i18n::Catalogs()) {
        names.push_back(catalog.name);
    }
    return names;
}

Row UiLanguage() {
    Row row{"game.ui_language", "UI language",
            "Language of the port's own menus. Auto follows the game language.\n"
            "Picking a language also sets the game language it names."};
    row.value = [](const Config &c) {
        const int index = UiLanguageIndex(c);
        return index == 0 ? std::string(kUiLanguageAuto) : i18n::Catalogs()[index - 1].name;
    };
    row.step = [](Config &c, int direction) {
        const int n = static_cast<int>(i18n::Catalogs().size()) + 1;
        SelectUiLanguage(c, (UiLanguageIndex(c) + direction + n) % n);
    };
    row.index = UiLanguageIndex;
    row.select = SelectUiLanguage;
    return row;
}

Row Resolution() {
    Row row{"video.width", "Resolution", "Window size. Fullscreen uses the desktop.\nCommand-line --window overrides this setting."};
    row.value = [](const Config &c) {
        if (c.window_width == 0 && c.window_height == 0) {
            return std::string("Desktop");
        }
        return std::format("{} x {}", c.window_width, c.window_height);
    };
    row.step = [](Config &c, int direction) {
        auto       sizes = ResolutionList(g_sizes, c.window_width, c.window_height);
        const auto it = std::ranges::find_if(sizes, [&](auto s) {
            return s.width == c.window_width && s.height == c.window_height;
        });
        const int  index = it == sizes.end() ? 0 : static_cast<int>(it - sizes.begin());
        auto       next = sizes[(index + direction + sizes.size()) % sizes.size()];
        c.window_width = next.width;
        c.window_height = next.height;
    };
    return row;
}

std::vector<Row> Bindings() {
    static constexpr const char *labels[] = {
        "D-pad Up",
        "D-pad Down",
        "D-pad Left",
        "D-pad Right",
    };
    std::vector<Row> rows;
    for (const auto &info : InputActions()) {
        if (info.whole_axis) {
            continue;
        }
        // These host actions have no game-side consumer in this port.
        if (info.action == "developer_menu" || info.action == "gyro_hold") {
            continue;
        }
        std::string      action(info.action);
        std::string      label = action;
        static constexpr std::pair<std::string_view, const char *> names[] = {
            {"cross",      "Cross"       },
            {"circle",     "Circle"      },
            {"square",     "Square"      },
            {"triangle",   "Triangle"    },
            {"up",         labels[0]     },
            {"down",       labels[1]     },
            {"left",       labels[2]     },
            {"right",      labels[3]     },
            {"start",      "Start"       },
            {"select",     "Select"      },
            {"l1",         "L1"          },
            {"r1",         "R1"          },
            {"l2",         "L2"          },
            {"r2",         "R2"          },
            {"l3",         "L3"          },
            {"r3",         "R3"          },
            {"lx-",        "Move left"   },
            {"lx+",        "Move right"  },
            {"ly-",        "Move forward"},
            {"ly+",        "Move back"   },
            {"rx-",        "Look left"   },
            {"rx+",        "Look right"  },
            {"ry-",        "Look up"     },
            {"ry+",        "Look down"   },
            {"zoom_reset", "Zoom reset"  },
            {"fps_toggle", "Toggle FPS"  },
            {"debug_menu", "Debug menu"  },
        };
        for (auto [name, text] : names) {
            if (action == name) {
                label = text;
            }
        }
        rows.push_back({"input.bindings." + action, label, "Confirm, then press a key or mouse button.\nEscape cancels. Square restores this binding.", [action](const Config &) { return InputBindingLabel(action); }, {}, action});
    }
    return rows;
}

// The pages without the catalog list, which the English table is built from before catalogs load.
std::vector<Page> &BuiltPages() {
    static std::vector<Page> pages{
        {"Display",  {
                        Choice<&Config::fullscreen>("video.fullscreen", "Window mode", "Applies immediately. Fullscreen uses the desktop.", {"Windowed", "Fullscreen"}),
                        Resolution(),
                        Choice<&Config::present_mode>("video.present_mode", "Present mode", "FIFO prevents tearing. Mailbox keeps the newest frame.\nUnsupported modes fall back to FIFO.", {"FIFO", "Mailbox", "Immediate"}),
                        Number<&Config::max_fps>("video.max_fps", "Frame cap", "Limits display frames, not game speed.", {0, 30, 50, 60, 75, 90, 100, 120, 144, 165, 180, 240, 360}, 1, " FPS"),
                         Choice<&Config::interpolation>("video.interpolation", "Interpolation", "Smooths display frames between game ticks.", {"Off", "On"}),
                         Number<&Config::interpolation_draw_budget>("video.interpolation_draw_budget",
                                                                    "Interpolation draw budget",
                                                                    "Skips tweened frames when a tick exceeds this many draws (0 = off).",
                                                                    {0, 2000, 2400, 2600, 3000, 3500, 4000, 5000, 8000}),
                        Choice<&Config::aspect>("video.aspect", "Aspect ratio", "Auto fills the window. 4:3 adds black side bars.", {"Auto", "4:3"}),
                        Number<&Config::ui_scale>("video.ui_scale", "UI scale", "Scales menus and HUD. Large values can crop the UI.\nUse Left to reduce the scale if it no longer fits.", {0.5, 0.65, 0.75, 0.85, 1, 1.1, 1.25, 1.5}, 100, "%"),
                        Choice<&Config::show_fps>("video.show_fps", "Show FPS", "Shows the display and tick rates in a window.", {"Off", "On"}),
                    }  },
        {"Audio",    {
                      Number<&Config::master_volume>("audio.master_volume", "Master volume", "Adjusts all sound, including music and movies.", {0, .05, .1, .15, .2, .25, .3, .35, .4, .45, .5, .55, .6, .65, .7, .75, .8, .85, .9, .95, 1}, 100, "%"),
                      Choice<&Config::soundtrack>("audio.soundtrack", "Soundtrack", "Applies when the next song starts; no restart needed.\nCustom needs soundtrack/soundtrack.json recordings.", {"PS2", "Custom"}),
                      Choice<&Config::dualsense_speaker>("audio.dualsense_speaker", "DualSense Speaker", "Plays sound effects on a DualSense controller speaker.\nTurn off if you want to keep vanilla experience", {"Off", "On"}),
                      Number<&Config::controller_volume>("audio.controller_volume", "Controller volume", "DualSense speaker volume, after master volume.", {0, .1, .2, .3, .4, .5, .6, .7, .8, .9, 1, 1.25, 1.5, 1.75, 2}, 100, "%"),
                  }      },
        {"Controls", {
                         Number<&Config::mouse_sensitivity>("input.mouse_sensitivity", "Mouse sensitivity", "Camera turn per count of mouse movement.", {.01, .025, .05, .075, .1, .15, .2, .3, .5, .75, 1}),
                         Choice<&Config::mouse_invert_y>("input.mouse_invert_y", "Invert mouse Y", "Reverses vertical mouse look.", {"Off", "On"}),
                         Choice<&Config::mouse_capture>("input.mouse_capture", "Capture mouse", "Keeps the pointer inside the game.\nRelease with a configured mouse-release key.", {"Off", "On"}),
                         Number<&Config::stick_sensitivity>("input.stick_sensitivity", "Stick sensitivity", "Scales controller stick response.", {.5, .75, 1, 1.1, 1.2, 1.33, 1.5, 1.75, 2, 2.5, 3}),
                         Choice<&Config::stick_invert_x>("input.stick_invert_x", "Invert stick X", "Reverses the controller look stick's horizontal axis.", {"Off", "On"}),
                         Choice<&Config::stick_invert_y>("input.stick_invert_y", "Invert stick Y", "Reverses the controller look stick's vertical axis.", {"Off", "On"}),
                         Choice<&Config::dualsense_haptics>("input.dualsense_haptics", "DualSense haptics", "Plays vibration on a DualSense's HD actuators.\nFull adds quakes and hit, weapon and monster effects.", {"Off", "Vibration", "Full"}),
                         Number<&Config::haptics_strength>("input.haptics_strength", "Haptics strength", "DualSense actuator strength.", {0, .25, .5, .75, 1, 1.25, 1.5, 1.75, 2}, 100, "%"),
                         Choice<&Config::glyphs_new>("input.glyphs", "Button symbols", "New redraws the game's button symbols from glyphs/.\nOriginal keeps the PlayStation art.", {"Original", "New"}),
                         Choice<&Config::glyph_device>("input.glyph_device", "Glyph style", "Auto follows the last device used.\nKeyboard prompts follow your current bindings.", {"Auto", "PS3", "PS4", "PS5", "PS5 Color", "Xbox", "Switch", "Steam Deck", "Steam Controller", "Keyboard"}),
                         Choice<&Config::mouse_zoom>("input.mouse_zoom", "Mouse wheel zoom", "Wheel changes camera distance while walking.\nMenus use the wheel for selection instead.", {"Off", "On"}),
                         Choice<&Config::mouse_fixed_look>("input.mouse_fixed_look", "Fixed camera mouse look", "Gently look around fixed-camera rooms.\nThe view returns when the mouse rests.", {"Off", "On"}),
                     }},
        {"Bindings", Bindings()           },
        {"Game",     {
                         Choice<&Config::autosave>("game.autosave", "Autosave",
                                                     "Saves on arriving in a town or on a dungeon floor.\n"
                                                     "The load screen lists it first.",
                                                     {"Off", "On"}),
                         Choice<&Config::quick_loadout_plate>("game.quick_loadout_title", "Quick loadout title",
                                                              "How the D-pad down panel shows its title:\n"
                                                              "on a bar, or on a plate like the EXP gauge's.",
                                                              {"Bar", "Plate"}),
                         Choice<&Config::debug_mode>("game.debug_mode", "Debug mode",
                                                     "Enables master debug behavior for this session and save.",
                                                     {"Off", "On"}),
                         Choice<&Config::skip_intro>("game.skip_intro", "Skip intro",
                                                     "Skips the Sony and Level-5 logos at boot and on the title screen.",
                                                     {"Off", "On"}),
                         GameLanguage(),
                         UiLanguage(),
                         Choice<&Config::expanded_dungeon_monsters>("game.expanded_dungeon_monsters",
                                                                    "Expanded dungeon monsters",
                                                                    "Raises the active monster cap from 24 to 60 and shows\n"
                                                                    "all of them in view (retail only keeps the 5 nearest visible).\n"
                                                                    "Re-enter the floor after changing this.",
                                                                    {"Off", "On"}),
                         []() {
                             Row row{"game.georama_poly_multiplier", "Georama budget",
                                     "Multiplies how many polygons you can place in Georama towns.\n"
                                     "Draw-memory limits stay at retail."};
                             row.value = [](const Config &c) { return std::format("{}×", c.georama_poly_multiplier); };
                             row.step = [](Config &c, int direction) {
                                 int next = c.georama_poly_multiplier + direction;
                                 if (next < 1) {
                                     next = 6;
                                 }
                                 if (next > 6) {
                                     next = 1;
                                 }
                                 c.georama_poly_multiplier = next;
                             };
                             row.choices = {"1×", "2×", "3×", "4×", "5×", "6×"};
                             row.numeric = true;
                             row.index = [](const Config &c) { return std::clamp(c.georama_poly_multiplier, 1, 6) - 1; };
                             row.select = [](Config &c, int index) { c.georama_poly_multiplier = std::clamp(index, 0, 5) + 1; };
                             return row;
                         }(),
                         Choice<&Config::dungeon_ceremony_qol>("game.dungeon_ceremony_qol", "Fast dungeon ceremonies",
                                                              "Off is retail pacing.\nManual: faster beats and tap "
                                                              "confirm to finish ceremony text.\nAuto: instant "
                                                              "ceremony text.",
                                                              {"Off", "Manual", "Auto"}),
                         Choice<&Config::dungeon_pickup_qol>("game.dungeon_pickup_qol", "Snappy dungeon pickups",
                                                             "Off is retail.\nOn: wider magnet, faster vacuum, loot "
                                                             "never fades, and leftover pickups collect at the exit door.",
                                                             {"Off", "On"}),
                         Choice<&Config::town_draw_distance>("game.town_draw_distance", "Town draw distance",
                                                             "Retail uses PS2 far cull distances.\nExtended draws town "
                                                             "NPCs and parts 1.5× farther while walking (default).",
                                                             {"Retail", "Extended"}),
                         Choice<&Config::scoop_tracker>("game.scoop_tracker", "Scoop photo tracker",
                                                          "Off is retail (no markers).\nMinimal: camera icon while a "
                                                          "scoop photo window is open.\nFull: icon plus scoop name.",
                                                          {"Off", "Minimal", "Full"}),
                         Choice<&Config::auto_use_event_item>("game.auto_use_event_item", "Auto-use event items",
                                                              "Default Off. When On, events that ask for one specific item\n"
                                                              "you already have skip the item picker grid.",
                                                              {"Off", "On"}),
                         Choice<&Config::floor_info_monsters>("game.floor_info_monsters", "Floor monster list",
                                                              "Triangle on a floor's info lists its monsters, drops and\n"
                                                              "gate key carrier (spoilers). Off is the original game.",
                                                              {"Off", "On"}),
                         Choice<&Config::abs_bank>("game.abs_bank", "Bank weapon EXP orbs",
                                                   "Off is retail: orbs feed equipped weapons.\n"
                                                   "On: orbs add to a per-save bank you can apply on weapon stats.",
                                                   {"Off", "On"}),
                         Choice<&Config::ui_preview>("game.ui_preview", "UI hub preview",
                                                     "Test build of the new menu, on sample data only.\n"
                                                     "On: F10, or Select with L1+R1 held, opens it in town or dungeons.",
                                                     {"Off", "On"}),
                         Choice<&Config::ui_theme>("game.ui_theme", "UI theme",
                                                   "The look of the new menu. Dark Cloud 2 uses the game's own\n"
                                                   "menu art and changes as soon as you pick it.",
                                                   {"Dark Cloud 2", "Chronicle", "Parchment"}),
                     }                      },
    };
    const auto names = port_settings::PcOverlayPageNames();
    assert(pages.size() == static_cast<std::size_t>(port_settings::PcOverlayPageCount()));
    for (std::size_t i = 0; i < pages.size(); ++i) {
        assert(pages[i].name == names[i]);
    }
    return pages;
}

std::string TextKey(const Row &row, std::string_view part) {
    if (part == "help" && row.key.starts_with("input.bindings.")) {
        return "options.input.bindings.help";
    }
    return std::format("options.{}.{}", row.key, part);
}

std::string PageKey(const Page &page) {
    std::string key = std::format("options.page.{}", page.name);
    std::ranges::transform(key, key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return key;
}

// Rows whose choices are numbers or names that are not translated.
bool ChoicesTranslate(const Row &row) {
    return !row.numeric && row.index && row.key != "game.ui_language";
}

} // namespace

std::span<const Page> Pages() {
    auto &pages = BuiltPages();
    // Catalogs can be reloaded after the pages are built.
    for (auto &page : pages) {
        for (auto &row : page.rows) {
            if (row.key == "game.ui_language") {
                row.choices = UiLanguageChoices();
            }
        }
    }
    return pages;
}

std::string_view PageName(const Page &page, i18n::Drawable drawable) {
    return i18n::Tr(PageKey(page), drawable);
}

std::string_view RowLabel(const Row &row, i18n::Drawable drawable) {
    return i18n::Tr(TextKey(row, "label"), drawable);
}

std::string_view RowHelp(const Row &row, i18n::Drawable drawable) {
    return i18n::Tr(TextKey(row, "help"), drawable);
}

std::string RowValue(const Row &row, const Config &config, i18n::Drawable drawable) {
    if (row.key == "video.width" && config.window_width == 0 && config.window_height == 0) {
        return std::string(i18n::Tr("options.video.width.desktop", drawable));
    }
    if (row.key == "game.ui_language") {
        const int index = UiLanguageIndex(config);
        if (index == 0) {
            return std::string(i18n::Tr("options.game.ui_language.auto", drawable));
        }
        const auto &catalog = i18n::Catalogs()[index - 1];
        return drawable == nullptr || drawable(catalog.name) ? catalog.name : catalog.tag;
    }
    if (ChoicesTranslate(row)) {
        const int index = row.index(config);
        if (index >= 0 && index < static_cast<int>(row.choices.size())) {
            return std::string(i18n::Tr(TextKey(row, std::format("choice.{}", index)), drawable));
        }
    }
    return row.value(config);
}

std::vector<std::pair<std::string, std::string>> EnglishText() {
    std::vector<std::pair<std::string, std::string>> text;
    for (const auto &page : BuiltPages()) {
        text.emplace_back(PageKey(page), page.name);
        for (const auto &row : page.rows) {
            text.emplace_back(TextKey(row, "label"), row.label);
            if (const auto help = TextKey(row, "help");
                std::ranges::find(text, help, &std::pair<std::string, std::string>::first) == text.end()) {
                text.emplace_back(help, row.help);
            }
            if (ChoicesTranslate(row)) {
                for (std::size_t i = 0; i < row.choices.size(); ++i) {
                    text.emplace_back(TextKey(row, std::format("choice.{}", i)), row.choices[i]);
                }
            }
        }
    }
    return text;
}

bool StepRow(const Row &row, Config &config, int direction) {
    if (!row.step || direction == 0) {
        return false;
    }
    const Config before = config;
    row.step(config, direction > 0 ? 1 : -1);
    return before != config;
}

bool SetBinding(Config &config, std::string_view action, std::string_view key) {
    if (key.empty() || InputScancodeFromName(key) == InputScancodeFromName("Escape")) {
        return false;
    }
    const bool mouse = key.size() == 6 && key.substr(0, 5) == "Mouse" && key[5] >= '1' && key[5] <= '5';
    if (InputScancodeFromName(key) < 0 && !mouse) {
        return false;
    }
    const auto actions = InputActions();
    const auto known = std::ranges::find(actions, action, &InputActionInfo::action);
    if (known == actions.end() || known->whole_axis) {
        return false;
    }
    auto it = std::ranges::find(config.key_bindings, action, &ConfigKeyBinding::action);
    if (it == config.key_bindings.end()) {
        config.key_bindings.push_back({std::string(action), {std::string(key)}});
    } else {
        it->keys = {std::string(key)};
    }
    return true;
}

void ResetBinding(Config &config, std::string_view action) {
    std::erase_if(config.key_bindings, [&](const auto &b) { return b.action == action; });
}

std::vector<DisplayModeSize> ResolutionList(std::span<const DisplayModeSize> modes, int width, int height) {
    std::vector<DisplayModeSize> sizes{
        {0,    0   },
        {800,  600 },
        {1024, 768 },
        {1280, 720 },
        {1280, 960 },
        {1920, 1080}
    };
    for (auto size : modes) {
        if (size.width >= 640 && size.height >= 480) {
            sizes.push_back(size);
        }
    }
    if (width != 0 || height != 0) {
        sizes.push_back({width, height});
    }
    std::ranges::sort(sizes, [](auto a, auto b) { return std::pair(a.width, a.height) < std::pair(b.width, b.height); });
    const auto duplicates = std::ranges::unique(sizes, [](auto a, auto b) { return a.width == b.width && a.height == b.height; });
    sizes.erase(duplicates.begin(), duplicates.end());
    return sizes;
}

void ListResolutions() {
    const auto &c = ConfigGet();
    g_sizes = ResolutionList(WindowDisplayModes(), c.window_width, c.window_height);
}

std::string ButtonName(std::string_view action, const Config &config) {
    auto device = config.glyph_device;
    if (!config.glyphs_new) {
        device = ConfigGlyphDevice::Ps3; // the PS2 pad's names, as the game's art shows them
    }
    if (device == ConfigGlyphDevice::Auto) {
        switch (InputActiveGlyphFamily()) {
            case InputGlyphFamily::Keyboard:
                device = ConfigGlyphDevice::Keyboard;
                break;
            case InputGlyphFamily::Xbox:
                device = ConfigGlyphDevice::Xbox;
                break;
            case InputGlyphFamily::Switch:
                device = ConfigGlyphDevice::Switch;
                break;
            case InputGlyphFamily::SteamDeck:
                device = ConfigGlyphDevice::SteamDeck;
                break;
            case InputGlyphFamily::SteamController:
                device = ConfigGlyphDevice::SteamController;
                break;
            default:
                device = ConfigGlyphDevice::Ps4;
                break;
        }
    }
    if (device == ConfigGlyphDevice::Keyboard) {
        const std::string bindings = InputBindingLabel(action);
        return bindings.substr(0, bindings.find(", "));
    }
    action = PadRemapGlyphFor(config.pad_remap, action);
    const bool xbox = device == ConfigGlyphDevice::Xbox || device == ConfigGlyphDevice::SteamDeck || device == ConfigGlyphDevice::SteamController;
    const bool sw = device == ConfigGlyphDevice::Switch;
    if (action == "cross") {
        return xbox ? "A" : sw ? "B"
                               : "Cross";
    }
    if (action == "circle") {
        return xbox ? "B" : sw ? "A"
                               : "Circle";
    }
    if (action == "square") {
        return xbox ? "X" : sw ? "Y"
                               : "Square";
    }
    if (action == "select") {
        return sw ? "Minus" : xbox ? "View" : device == ConfigGlyphDevice::Ps3 ? "Select" : "Touchpad";
    }
    if (action == "l1") {
        return xbox ? "LB" : sw ? "L"
                                : "L1";
    }
    if (action == "r1") {
        return xbox ? "RB" : sw ? "R"
                                : "R1";
    }
    return std::string(action);
}

} // namespace options
