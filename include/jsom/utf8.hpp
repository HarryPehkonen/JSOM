#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace jsom::utf8 {

/// Is this byte a UTF-8 continuation byte (10xxxxxx)?
[[nodiscard]] constexpr auto is_continuation(unsigned char byte) -> bool {
    return (byte & 0xC0) == 0x80;
}

/// Decodes the UTF-8 sequence starting at `index` into `codepoint`.
///
/// Returns the number of bytes consumed (1-4), or 0 when the sequence is not valid UTF-8
/// — a bad lead byte, a missing or malformed continuation byte, an overlong encoding, a
/// surrogate half (U+D800..U+DFFF, which UTF-8 must never encode), or a value above
/// U+10FFFF. Callers decide what to do with invalid input; nothing is thrown here.
[[nodiscard]] inline auto decode(std::string_view text, std::size_t index, std::uint32_t& codepoint)
    -> std::size_t {
    if (index >= text.size()) {
        return 0;
    }

    const auto lead = static_cast<unsigned char>(text[index]);

    std::size_t length = 0;
    std::uint32_t value = 0;
    if (lead < 0x80) {
        codepoint = lead;
        return 1;
    } else if ((lead & 0xE0) == 0xC0) {
        length = 2;
        value = lead & 0x1F;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
        value = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
        value = lead & 0x07;
    } else {
        return 0; // 0x80..0xBF (stray continuation) or 0xF8+ (never valid in UTF-8)
    }

    if (index + length > text.size()) {
        return 0; // truncated sequence
    }

    for (std::size_t offset = 1; offset < length; ++offset) {
        const auto byte = static_cast<unsigned char>(text[index + offset]);
        if (!is_continuation(byte)) {
            return 0;
        }
        value = (value << 6) | (byte & 0x3F);
    }

    // Overlong encodings, UTF-16 surrogate halves and out-of-range values are not UTF-8.
    const std::uint32_t minimum = (length == 2) ? 0x80U : (length == 3) ? 0x800U : 0x10000U;
    if (value < minimum || value > 0x10FFFFU) {
        return 0;
    }
    if (value >= 0xD800U && value <= 0xDFFFU) {
        return 0;
    }

    codepoint = value;
    return length;
}

/// Appends `codepoint` to `out` as UTF-8 (the inverse of decode()).
inline void encode(std::string& out, std::uint32_t codepoint) {
    if (codepoint <= 0x7FU) {
        out += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FFU) {
        out += static_cast<char>(0xC0U | (codepoint >> 6));
        out += static_cast<char>(0x80U | (codepoint & 0x3FU));
    } else if (codepoint <= 0xFFFFU) {
        out += static_cast<char>(0xE0U | (codepoint >> 12));
        out += static_cast<char>(0x80U | ((codepoint >> 6) & 0x3FU));
        out += static_cast<char>(0x80U | (codepoint & 0x3FU));
    } else {
        out += static_cast<char>(0xF0U | (codepoint >> 18));
        out += static_cast<char>(0x80U | ((codepoint >> 12) & 0x3FU));
        out += static_cast<char>(0x80U | ((codepoint >> 6) & 0x3FU));
        out += static_cast<char>(0x80U | (codepoint & 0x3FU));
    }
}

} // namespace jsom::utf8
