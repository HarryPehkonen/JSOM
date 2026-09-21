#pragma once

/// The JSOM adapter for JSONFuzz: a jsonfuzz::SystemUnderTest wrapping jsom::JsonDocument.
///
/// Strings cross the boundary; no parser type is shared. parse() maps ParseConfig onto
/// jsom::JsonParseOptions and reports rejection as an outcome (accepted=false + the
/// message), never a throw. serialize() returns to_json() (compact, the default);
/// pointers(depth) returns list_paths(depth); get(pointer) returns the value at that
/// pointer serialized, or nullopt when it does not resolve.

#include <jsom/jsom.hpp>
#include <jsonfuzz/sut.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jsom_fuzz {

class JsonomSut : public jsonfuzz::SystemUnderTest {
public:
    jsonfuzz::ParseResult parse(std::string_view text,
                                const jsonfuzz::ParseConfig& config) override;
    std::string serialize() const override;
    std::vector<std::string> pointers(int max_depth) const override;
    std::optional<std::string> get(std::string_view pointer) const override;

private:
    jsom::JsonDocument doc_;
    bool has_ = false;
};

} // namespace jsom_fuzz
