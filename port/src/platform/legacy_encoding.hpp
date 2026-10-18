#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace legacy_encoding {
/**
 * Encodes readable JSON text as the CP932 bytes expected by the game's fonts and identifiers.
 */
std::string GameText(std::string_view utf8);

// A game string (an item name) as UTF-8. ASCII passes through. A [UNI0xxx] tag becomes the character it names, and a
// letter byte becomes its Latin-1 letter, except the game's bytes for Œ and œ.
[[nodiscard]] std::string NameToUtf8(std::string_view text);

// The 16-bit glyph a character encodes as in a PAL message, the mapping message text uses. Newline and space are
// control codes, not glyphs, so they are empty here.
[[nodiscard]] std::optional<std::uint16_t> GlyphCode(char32_t ch);

// Names and numbers a message inserts, in the order its insertion codes address them. sign prefixes a positive
// number with '+'; show_zero keeps a zero that would otherwise be left out. primary is the message's own value.
struct MessageInserts {
    std::array<std::string_view, 16> names{};
    std::array<int, 16>              numbers{};
    int                              primary = 0;
    bool                             sign = true;
    bool                             show_zero = false;
};

// A PAL message (16-bit glyph and control codes, ending at the end code) as UTF-8. Insertions are filled from
// inserts. A code with no character is left out, with one warning per code per session.
[[nodiscard]] std::string MessageToUtf8(const std::uint16_t *codes, const MessageInserts &inserts = {});
} // namespace legacy_encoding
