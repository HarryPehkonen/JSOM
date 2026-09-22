#pragma once

/// The one place a JSON string is escaped.
///
/// Why this header exists (measured 2026-09-21): JSOM carried three implementations of this
/// rule — two private statics in json_document.hpp, one per output sink, and a third inside
/// the formatter. They had drifted: the string sink built `\uXXXX` by hand, the ostream sink
/// formatted it with `std::hex` / `std::setfill('0')` / `std::setw(4)`, and the formatter's
/// copy saved and restored the stream flags but still leaked the fill character. The ostream
/// one corrupted its caller's stream (42 printed as "2a"). One rule, one implementation, two
/// sinks, no manipulators.

#include "constants.hpp"
#include "utf8.hpp"

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

namespace jsom::escape {

/// What to do with bytes above ASCII when escaping.
enum class NonAscii : std::uint8_t {
    /// Pass them through: raw UTF-8 in a JSON string is valid, and this is what the
    /// serializer does (it never rewrites bytes it was not asked to rewrite).
    KeepRaw,
    /// Escape them as `\uXXXX` codepoints. Non-ASCII CODEPOINTS, never UTF-8 bytes:
    /// escaping the bytes of "ä" gives "\u00c3\u00a4", which reads back as two different
    /// characters.
    EscapeCodepoints,
};

namespace detail {

/// `\uXXXX` with lowercase hex, or a surrogate pair above U+FFFF (the only form a JSON
/// reader can decode). Written by hand rather than with stream manipulators, which are
/// sticky and leak into whatever the caller prints next.
inline void write_hex_escape(std::string& out, std::uint32_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    if (value > 0xFFFFU) {
        // Above the BMP: two escapes, and neither gets the prefix from here (the recursion
        // writes its own). Emitting it before this branch produced `\u\ud83d\ude00`.
        const std::uint32_t offset = value - 0x10000U;
        write_hex_escape(out, 0xD800U + (offset >> 10));
        write_hex_escape(out, 0xDC00U + (offset & 0x3FFU));
        return;
    }
    out += "\\u";
    out += digits[(value >> 12) & 0xFU];
    out += digits[(value >> 8) & 0xFU];
    out += digits[(value >> 4) & 0xFU];
    out += digits[value & 0xFU];
}

} // namespace detail

/// Does this text contain anything that must be escaped (or, in `EscapeCodepoints` mode,
/// anything non-ASCII that should be)? One scan, so the common case can be a plain append.
[[nodiscard]] inline auto needs_escaping(std::string_view text, NonAscii non_ascii) -> bool {
    for (const char raw : text) {
        // NOLINTNEXTLINE(readability-identifier-length) -- `b` is the measured byte
        const auto b = static_cast<unsigned char>(raw);
        if (raw == '"' || raw == '\\' || b < character_constants::MIN_CONTROL_CHAR) {
            return true;
        }
        if (non_ascii == NonAscii::EscapeCodepoints && b > character_constants::MAX_ASCII_CHAR) {
            return true;
        }
    }
    return false;
}

/// Append `text` to `out` as the inside of a JSON string (no surrounding quotes).
///
/// Control characters are always escaped whatever the mode: a raw one is not valid JSON,
/// and the parser refuses it on input, so an in-memory document is the only way to have
/// one — and it still must not reach the output raw.
inline void append(std::string& out, std::string_view text, NonAscii non_ascii) {
    if (!needs_escaping(text, non_ascii)) {
        out.append(text);
        return;
    }
    out.reserve(out.size() + text.size() + 8);
    for (std::size_t i = 0; i < text.size(); ++i) {
        // NOLINTNEXTLINE(readability-identifier-length) -- loop-local measured byte
        const char raw = text[i];
        const auto b = static_cast<unsigned char>(raw);
        switch (raw) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (b < character_constants::MIN_CONTROL_CHAR) {
                detail::write_hex_escape(out, b);
            } else if (non_ascii == NonAscii::EscapeCodepoints
                       && b > character_constants::MAX_ASCII_CHAR) {
                std::uint32_t codepoint = 0;
                const std::size_t consumed = utf8::decode(text, i, codepoint);
                if (consumed == 0) {
                    // Invalid UTF-8. Escaping the byte as a codepoint would CHANGE the value
                    // (`\u00c3` decodes to two bytes), so the byte is passed through: a string
                    // that was not valid UTF-8 stays exactly as it was, and the output is no
                    // more invalid than the input. Validating input UTF-8 is separate work.
                    out += raw;
                } else {
                    detail::write_hex_escape(out, codepoint);
                    i += consumed - 1; // the loop increments once more
                }
            } else {
                out += raw;
            }
            break;
        }
    }
}

/// The same, for a caller writing to a stream. Never touches the stream's state: the
/// previous implementation armed `std::hex` and `std::setfill('0')` on the caller's stream,
/// so the caller's next `<< 42` printed "2a".
inline void append(std::ostream& out, std::string_view text, NonAscii non_ascii) {
    if (!needs_escaping(text, non_ascii)) {
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        return;
    }
    std::string escaped;
    append(escaped, text, non_ascii);
    out.write(escaped.data(), static_cast<std::streamsize>(escaped.size()));
}

} // namespace jsom::escape
