#include "jsom/json_document.hpp"
#include "jsom/json_formatter.hpp"

namespace jsom {

auto JsonDocument::to_json(const JsonFormatOptions& options) const -> std::string {
    JsonFormatter formatter(options);
    return formatter.format(*this);
}

} // namespace jsom