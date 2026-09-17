#pragma once

#include "fast_parser.hpp"
#include "json_document.hpp"
#include "json_parse_options.hpp"
#include <string>

namespace jsom {

/// Parses JSON text into a document with the default options.
///
/// There is exactly one parser: FastParser, a direct-construction recursive-descent
/// parser. A second, event-based implementation (StreamingParser, with builders that
/// reassembled a document from its events) was deleted on 2026-09-16 — it produced the
/// same model as this function while being slower, had no callers, no fuzz coverage, and
/// answered "is this valid JSON?" differently from the parser below. One parser, one set
/// of rules.
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
