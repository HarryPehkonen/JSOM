#pragma once

#include "fast_parser.hpp"
#include "json_document.hpp"
#include "json_parse_options.hpp"
#include <string>

namespace jsom {

/// Parses JSON text into a document with the default options.
///
/// There is exactly one parser: FastParser, a direct-construction recursive-descent
/// parser. One parser, one set of rules — a lookup here and a lookup anywhere else in
/// JSOM answer "is this valid JSON?" the same way.
inline auto parse_document(const std::string& json) -> JsonDocument {
    FastParser parser;
    return parser.parse(json);
}

/// Parses JSON text into a document with explicit options (see JsonParseOptions).
inline auto parse_document(const std::string& json, const JsonParseOptions& options)
    -> JsonDocument {
    FastParser parser(options);
    return parser.parse(json);
}

} // namespace jsom
