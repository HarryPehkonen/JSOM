#pragma once

#include "batch_parser.hpp"
#include "core_types.hpp"
#include "fast_parser.hpp"
#include "json_document.hpp"
#include "json_format_options.hpp"
#include "json_formatter.hpp"
#include "json_parse_options.hpp"
#include "parse_events.hpp"
#include "path_node.hpp"
#include "streaming_parser.hpp"

namespace jsom {

using Document = JsonDocument;

namespace literals {
inline auto operator""_jsom(const char* str, std::size_t len) -> JsonDocument {
    return parse_document(std::string(str, len));
}
} // namespace literals

} // namespace jsom
