#include "jsonfuzz_jsom.hpp"

#include <exception>
#include <utility>

namespace jsom_fuzz {

jsonfuzz::ParseResult JsonomSut::parse(std::string_view text, const jsonfuzz::ParseConfig& config) {
    jsom::JsonParseOptions opts;
    opts.allow_comments = config.allow_comments;
    opts.convert_unicode_escapes = config.unicode_escapes;
    opts.validate_numbers = config.strict_numbers;
    opts.max_depth = config.max_depth;
    try {
        doc_ = jsom::parse_document(std::string(text), opts);
        has_ = true;
        return {true, ""};
    } catch (const std::exception& e) {
        has_ = false;
        return {false, e.what()};
    } catch (...) {
        has_ = false;
        return {false, "unknown parse error"};
    }
}

std::string JsonomSut::serialize() const {
    if (!has_) {
        return "";
    }
    return doc_.to_json();
}

std::vector<std::string> JsonomSut::pointers(int max_depth) const {
    if (!has_) {
        return {};
    }
    return doc_.list_paths(max_depth);
}

std::optional<std::string> JsonomSut::get(std::string_view pointer) const {
    if (!has_) {
        return std::nullopt;
    }
    const jsom::JsonDocument* node = doc_.find(std::string(pointer));
    if (node == nullptr) {
        return std::nullopt;
    }
    return node->to_json();
}

} // namespace jsom_fuzz
