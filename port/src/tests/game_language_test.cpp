#include <gtest/gtest.h>

#include "game_language.hpp"
#include "mainloop.hpp"
#include "platform/config.hpp"

TEST(GameLanguage, ParsesPalLanguageNames) {
    EXPECT_EQ(ConfigParseGameLanguage("english"), LANG_ENGLISH);
    EXPECT_EQ(ConfigParseGameLanguage("french"), LANG_FRENCH);
    EXPECT_EQ(ConfigParseGameLanguage("german"), LANG_GERMAN);
    EXPECT_EQ(ConfigParseGameLanguage("italian"), LANG_ITALIAN);
    EXPECT_EQ(ConfigParseGameLanguage("spanish"), LANG_SPANISH);
    EXPECT_EQ(ConfigParseGameLanguage("japanese"), 0);
    EXPECT_EQ(ConfigParseGameLanguage(""), 0);
}

TEST(GameLanguage, ConfigRoundTripOmitsUnsetLanguage) {
    Config config;
    EXPECT_EQ(config.game_language, 0);
    const std::string unset = ConfigSerialize(config);
    EXPECT_EQ(unset.find("language"), std::string::npos);

    config.game_language = LANG_SPANISH;
    const Config parsed = ConfigParse(ConfigSerialize(config));
    EXPECT_EQ(parsed.game_language, LANG_SPANISH);
    EXPECT_EQ(ConfigGameLanguageString(parsed.game_language), "spanish");
}

TEST(GameLanguage, InvalidLanguageStringKeepsDefault) {
    const Config config = ConfigParse(R"({"game": {"language": "klingon"}})");
    EXPECT_EQ(config.game_language, 0);
}
