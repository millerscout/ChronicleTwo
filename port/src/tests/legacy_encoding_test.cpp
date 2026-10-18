#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "options/rows.hpp"
#include "platform/config.hpp"
#include "platform/legacy_encoding.hpp"

namespace {

std::vector<std::uint16_t> Encode(std::string_view utf8) {
    std::vector<std::uint16_t> codes;
    for (std::size_t at = 0; at < utf8.size();) {
        const auto lead = static_cast<unsigned char>(utf8[at]);
        char32_t   ch = lead;
        std::size_t length = 1;
        if (lead >= 0xC0 && lead < 0xE0) {
            ch = (lead & 0x1F) << 6 | (static_cast<unsigned char>(utf8[at + 1]) & 0x3F);
            length = 2;
        }
        if (ch == '\n') {
            codes.push_back(0xFF00);
        } else if (ch == ' ') {
            codes.push_back(0xFF02);
        } else if (const auto code = legacy_encoding::GlyphCode(ch)) {
            codes.push_back(*code);
        }
        at += length;
    }
    codes.push_back(0xFF01);
    return codes;
}

} // namespace

TEST(LegacyEncoding, NamesDecodeTagsAndLoadedLetters) {
    EXPECT_EQ(legacy_encoding::NameToUtf8("Gr[UNI00fc]n"), "Grün");
    EXPECT_EQ(legacy_encoding::NameToUtf8("[UNI00e9]p[UNI00e9]e"), "épée");
    EXPECT_EQ(legacy_encoding::NameToUtf8("ca[UNI00f1][UNI00f3]n"), "cañón");
    EXPECT_EQ(legacy_encoding::NameToUtf8("[UNI0152]uvre"), "Œuvre");

    const char loaded[] = {'c', 'o', static_cast<char>(0xBD), 'u', 'r', static_cast<char>(0xE9), '\0'};
    EXPECT_EQ(legacy_encoding::NameToUtf8(loaded), "coŒuré");
}

TEST(LegacyEncoding, MessagesRoundTripAndDropCodes) {
    const std::string text = "Héllo\nWorld!";
    const auto        codes = Encode(text);
    EXPECT_EQ(legacy_encoding::MessageToUtf8(codes.data()), text);

    const std::uint16_t coloured[] = {0xFC01, 0x28, 0xFC00, 0xFF01}; // 'H' is 0x48-0x20 = 0x28
    EXPECT_EQ(legacy_encoding::MessageToUtf8(coloured), "H");
}

TEST(LegacyEncoding, MessagesFillInsertionsAndNameButtons) {
    const std::uint16_t codes[] = {0xFAF9, 0xFF02, 0xFBFA, 0xFF02, 0xFD06, 0xFF01};
    legacy_encoding::MessageInserts inserts;
    inserts.names[0] = "Bread";
    inserts.numbers[0] = 30;
    const std::string expected = "Bread +30 " + options::ButtonName("cross", ConfigGet());
    EXPECT_EQ(legacy_encoding::MessageToUtf8(codes, inserts), expected);

    inserts.numbers[0] = 0;
    EXPECT_EQ(legacy_encoding::MessageToUtf8(codes, inserts).find("30"), std::string::npos);
    inserts.show_zero = true;
    EXPECT_NE(legacy_encoding::MessageToUtf8(codes, inserts).find('0'), std::string::npos);
}

TEST(LegacyEncoding, UnknownMessageCodeWarnsOnce) {
    const std::uint16_t codes[] = {0x28, 0xF600, 0xFF01};
    testing::internal::CaptureStderr();
    EXPECT_EQ(legacy_encoding::MessageToUtf8(codes), "H");
    const std::string first = testing::internal::GetCapturedStderr();
    EXPECT_NE(first.find("F600"), std::string::npos);

    testing::internal::CaptureStderr();
    EXPECT_EQ(legacy_encoding::MessageToUtf8(codes), "H");
    EXPECT_EQ(testing::internal::GetCapturedStderr().find("F600"), std::string::npos);
}
