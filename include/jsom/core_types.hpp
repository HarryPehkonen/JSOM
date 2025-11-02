#pragma once

#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace jsom {

enum class JsonType : uint8_t { Null, Boolean, Number, String, Object, Array };

class TypeException : public std::runtime_error {
public:
    explicit TypeException(const std::string& message) : std::runtime_error(message) {}
};

class LazyNumber {
private:
    std::optional<std::string> original_repr_;
    mutable std::optional<double> cached_value_;

public:
    explicit LazyNumber(const std::string& repr) : original_repr_(repr) {}
    explicit LazyNumber(double value) : cached_value_(value) {}
    explicit LazyNumber(int value) : cached_value_(static_cast<double>(value)) {}

    [[nodiscard]] auto as_double() const -> double {
        if (cached_value_) {
            return *cached_value_;
        }

        if (!original_repr_) {
            throw TypeException("LazyNumber has no value to convert");
        }

        try {
            std::size_t pos;
            double value = std::stod(*original_repr_, &pos);
            if (pos != original_repr_->length()) {
                throw std::invalid_argument("Invalid number format");
            }
            cached_value_ = value;
            return value;
        } catch (const std::exception&) {
            throw TypeException("Cannot convert '" + *original_repr_ + "' to double");
        }
    }

    [[nodiscard]] auto as_int() const -> int {
        // NOLINTNEXTLINE(readability-identifier-length)
        double d = as_double();
        if (d != static_cast<int>(d)) {
            std::string repr = original_repr_ ? *original_repr_ : std::to_string(d);
            throw TypeException("Cannot convert '" + repr + "' to int (not an integer value)");
        }
        return static_cast<int>(d);
    }

    [[nodiscard]] auto as_long_long() const -> long long {
        // NOLINTNEXTLINE(readability-identifier-length)
        double d = as_double();
        if (d != static_cast<double>(static_cast<long long>(d))) {
            std::string repr = original_repr_ ? *original_repr_ : std::to_string(d);
            throw TypeException("Cannot convert '" + repr
                                + "' to long long (not an integer value)");
        }
        return static_cast<long long>(d);
    }

    [[nodiscard]] auto as_string() const -> std::string {
        if (original_repr_) {
            return *original_repr_;
        }
        if (cached_value_) {
            if (*cached_value_ == static_cast<int>(*cached_value_)) {
                return std::to_string(static_cast<int>(*cached_value_));
            }

            std::ostringstream oss;
            oss << *cached_value_;
            return oss.str();
        }
        throw TypeException("LazyNumber has no value to convert to string");
    }

    void serialize(std::ostream& out) const {
        if (original_repr_) {
            out << *original_repr_;
        } else if (cached_value_) {
            if (*cached_value_ == static_cast<int>(*cached_value_)) {
                out << static_cast<int>(*cached_value_);
            } else {
                out << *cached_value_;
            }
        } else {
            throw TypeException("LazyNumber has no value to serialize");
        }
    }

    [[nodiscard]] auto has_original_repr() const -> bool { return original_repr_.has_value(); }

    [[nodiscard]] auto get_original_repr() const -> const std::string& {
        if (!original_repr_) {
            throw TypeException("LazyNumber has no original representation");
        }
        return *original_repr_;
    }

    auto operator==(const LazyNumber& other) const -> bool {
        try {
            return as_double() == other.as_double();
        } catch (const TypeException&) {
            if (original_repr_ && other.original_repr_) {
                return *original_repr_ == *other.original_repr_;
            }
            return false;
        }
    }

    [[nodiscard]] auto is_integer() const -> bool {
        try {
            // NOLINTNEXTLINE(readability-identifier-length)
            double d = as_double();
            return d == static_cast<int>(d);
        } catch (const TypeException&) {
            return false;
        }
    }
};

} // namespace jsom
