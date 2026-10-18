#include "platform/legacy_encoding.hpp"

#include <set>
#include <stdexcept>
#include <vector>

#include "options/rows.hpp"
#include "platform/config.hpp"
#include "platform/log.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <iconv.h>
#endif

namespace legacy_encoding {
#ifdef _WIN32
namespace {
constexpr UINT kCodePageCp932 = 932;
} // namespace
#endif

std::string GameText(std::string_view utf8) {
    if (utf8.contains('\0')) {
        throw std::runtime_error("config string contains a null byte");
    }
    if (utf8.empty()) {
        return {};
    }
#ifdef _WIN32
    const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (!length) {
        throw std::runtime_error("config string is not valid UTF-8");
    }
    std::vector<wchar_t> wide(length);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
    BOOL       substituted = FALSE;
    const auto count = WideCharToMultiByte(kCodePageCp932, WC_NO_BEST_FIT_CHARS, wide.data(), length, nullptr, 0, nullptr, &substituted);
    if (!count || substituted) {
        throw std::runtime_error("config string cannot be represented in CP932");
    }
    std::string encoded(count, '\0');
    WideCharToMultiByte(kCodePageCp932, WC_NO_BEST_FIT_CHARS, wide.data(), length, encoded.data(), count, nullptr, &substituted);
    return encoded;
#else
    auto converter = iconv_open("CP932", "UTF-8");
    if (converter == reinterpret_cast<iconv_t>(-1)) {
        throw std::runtime_error("CP932 encoding unavailable");
    }
    std::string encoded(utf8.size() * 2 + 4, '\0');
    char       *input = const_cast<char *>(utf8.data());
    char       *output = encoded.data();
    auto        input_size = utf8.size();
    auto        output_size = encoded.size();
    const auto  result = iconv(converter, &input, &input_size, &output, &output_size);
    iconv_close(converter);
    if (result == static_cast<std::size_t>(-1) || input_size) {
        throw std::runtime_error("config string cannot be represented in CP932");
    }
    encoded.resize(encoded.size() - output_size);
    return encoded;
#endif
}

namespace {

void AppendUtf8(std::string &out, char32_t ch) {
    if (ch < 0x80) {
        out += static_cast<char>(ch);
    } else if (ch < 0x800) {
        out += static_cast<char>(0xC0 | (ch >> 6));
        out += static_cast<char>(0x80 | (ch & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (ch >> 12));
        out += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (ch & 0x3F));
    }
}

// [UNI0xxx]: '[', "UNI0", three hex digits, ']'.
bool FontTag(std::string_view text, std::size_t at, char32_t &ch) {
    if (at + 9 > text.size() || text.substr(at, 5) != "[UNI0" || text[at + 8] != ']') {
        return false;
    }
    ch = 0;
    for (int i = 0; i < 3; ++i) {
        const char digit = text[at + 5 + i];
        const int  value = digit >= '0' && digit <= '9' ? digit - '0'
                           : digit >= 'a' && digit <= 'f' ? digit - 'a' + 10
                           : digit >= 'A' && digit <= 'F' ? digit - 'A' + 10
                                                           : -1;
        if (value < 0) {
            return false;
        }
        ch = (ch << 4) | static_cast<char32_t>(value);
    }
    return true;
}

constexpr std::uint16_t kNewline = 0xFF00;
constexpr std::uint16_t kEnd = 0xFF01;
constexpr std::uint16_t kSpace = 0xFF02;

// The number an insertion code addresses: -1 is the message's own value, else an index into the given numbers.
// These are the code ranges the message window reads as numbers.
std::optional<int> NumberSlot(std::uint16_t code) {
    if (code < 0xFB00) {
        return std::nullopt;
    }
    const int low = code - 0xFB00;
    if (low == 0xFF) {
        return -1;
    }
    if (low >= 0xF3 && low < 0xFB) {
        return 0xFA - low;
    }
    if (low >= 0xDF && low < 0xE7) {
        return 0xEE - low;
    }
    return std::nullopt;
}

bool Dropped(std::uint16_t code) {
    const auto in = [&](std::uint16_t from, std::uint16_t to) { return code >= from && code < to; };
    return in(0xF200, 0xF600) || in(0xF700, 0xFA00) || in(0xFC00, 0xFD00) || in(0xFE00, 0xFF00) ||
           (code >= 0xFF03 && code <= 0xFF06);
}

std::string_view ButtonGlyph(std::uint16_t code) {
    switch (code) {
        case 0xFD06:
            return "cross";
        case 0xFD07:
            return "triangle";
        case 0xFD08:
            return "circle";
        case 0xFD09:
            return "square";
        default:
            return {};
    }
}

void WarnUnknown(std::uint16_t code) {
    static std::set<std::uint16_t> warned;
    if (!warned.insert(code).second) {
        return;
    }
    LogFormat(LogLevel::Warn, "ui", "message code {:04X} has no character", code);
}

void AppendNumber(std::string &out, int value, const MessageInserts &inserts) {
    if (value == 0 && !inserts.show_zero) {
        return;
    }
    if (inserts.sign && value > 0) {
        out += '+';
    }
    out += std::to_string(value);
}

} // namespace

std::string NameToUtf8(std::string_view text) {
    std::string out;
    for (std::size_t at = 0; at < text.size();) {
        char32_t tag = 0;
        if (FontTag(text, at, tag)) {
            AppendUtf8(out, tag);
            at += 9;
            continue;
        }
        const auto byte = static_cast<unsigned char>(text[at++]);
        if (byte < 0x80) {
            out += static_cast<char>(byte);
        } else if (byte == 0xBD) {
            AppendUtf8(out, U'\u0152');
        } else if (byte == 0xBE) {
            AppendUtf8(out, U'\u0153');
        } else {
            AppendUtf8(out, byte);
        }
    }
    return out;
}

std::optional<char32_t> Glyph(std::uint16_t code) {
    if (code > 0 && code < 0x3C) {
        return static_cast<char32_t>(code + 0x20);
    }
    if (code >= 0x3C && code <= 0x5D) {
        return static_cast<char32_t>(code + 0x21);
    }
    if (code >= 0xFFA0) {
        return code & 0xFF;
    }
    return std::nullopt;
}

std::optional<std::uint16_t> GlyphCode(char32_t ch) {
    const auto code = ch > 0x20 && ch < 0x5C     ? static_cast<std::uint16_t>(ch - 0x20)
                      : ch > 0x5C && ch < 0x7F   ? static_cast<std::uint16_t>(ch - 0x21)
                      : ch >= 0xA0 && ch <= 0xFF ? static_cast<std::uint16_t>(0xFF00 | ch)
                                                 : std::uint16_t{0};
    return code != 0 && Glyph(code) == ch ? std::optional<std::uint16_t>{code} : std::nullopt;
}

std::string MessageToUtf8(const std::uint16_t *codes, const MessageInserts &inserts) {
    std::string out;
    if (codes == nullptr) {
        return out;
    }
    for (const std::uint16_t *code = codes; *code != kEnd; ++code) {
        if (*code == kNewline) {
            out += '\n';
        } else if (*code == kSpace) {
            out += ' ';
        } else if (const auto glyph = Glyph(*code)) {
            AppendUtf8(out, *glyph);
        } else if (*code >= 0xFAEA && *code <= 0xFAF9) {
            out += inserts.names[static_cast<std::size_t>(0xFAF9 - *code)];
        } else if (const auto slot = NumberSlot(*code)) {
            AppendNumber(out, *slot < 0 ? inserts.primary : inserts.numbers[static_cast<std::size_t>(*slot)], inserts);
        } else if (const std::string_view button = ButtonGlyph(*code); !button.empty()) {
            out += options::ButtonName(button, ConfigGet());
        } else if (!Dropped(*code)) {
            WarnUnknown(*code);
        }
    }
    return out;
}

} // namespace legacy_encoding
