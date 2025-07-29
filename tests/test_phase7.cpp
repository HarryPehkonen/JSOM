#include "jsom.hpp"
#include <gtest/gtest.h>
#include <vector>

class Phase7Test : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<jsom::StreamingParser>();
        values_.clear();
        errors_.clear();

        // Set up event callbacks
        jsom::ParseEvents events;
        events.on_value = [this](const jsom::JsonValue& value) {
            values_.emplace_back(value.type(), value.raw_value(), value.path());
        };
        events.on_error = [this](const jsom::ParseError& error) {
            errors_.emplace_back(error.position, error.message, error.json_pointer);
        };

        parser->set_events(events);
    }

    void parse(const std::string& json) {
        parser->parse_string(json);
        parser->end_input();
    }

    std::unique_ptr<jsom::StreamingParser> parser;
    std::vector<std::tuple<jsom::JsonType, std::string, std::string>> values_;
    std::vector<std::tuple<std::size_t, std::string, std::string>> errors_;
};

// Test parser reuse with reset
TEST_F(Phase7Test, ParserReuse) {
    // Parse first JSON
    parse(R"({"first": 123})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<1>(values_[0]), "123");
    EXPECT_EQ(std::get<2>(values_[0]), "/first");

    // Reset parser
    parser->reset();
    values_.clear();
    errors_.clear();

    // Parse second JSON with same parser instance
    parse(R"({"second": "hello"})");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 1);
    EXPECT_EQ(std::get<1>(values_[0]), "hello");
    EXPECT_EQ(std::get<2>(values_[0]), "/second");
}

// Test end_input validation
TEST_F(Phase7Test, EndInputValidation) {
    // Test unclosed object
    parser->parse_string(R"({"incomplete":)");
    parser->end_input();

    ASSERT_GE(errors_.size(), 1);
    // Should report unclosed container error
    bool found_unclosed_error = false;
    for (const auto& error : errors_) {
        if (std::get<1>(error).find("unclosed containers") != std::string::npos) {
            found_unclosed_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_unclosed_error);
}

// Test incomplete string detection
TEST_F(Phase7Test, IncompleteStringDetection) {
    parser->parse_string(R"({"key": "unterminated)");
    parser->end_input();

    ASSERT_GE(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Unterminated string at end of input");
}

// Test incomplete literal detection
TEST_F(Phase7Test, IncompleteLiteralDetection) {
    parser->parse_string("tru");
    parser->end_input();

    ASSERT_GE(errors_.size(), 1);
    EXPECT_EQ(std::get<1>(errors_[0]), "Invalid literal");
}

// Test error message context
TEST_F(Phase7Test, ErrorMessageContext) {
    parse(R"({"nested": {"error": invalid_value}})");

    ASSERT_GE(errors_.size(), 1);
    // Error should include JSON Pointer context
    EXPECT_EQ(std::get<2>(errors_[0]), "/nested/error");
}

// Test convenience parse_string method
TEST_F(Phase7Test, ParseStringConvenience) {
    // Should work identically to character-by-character feeding
    parse(R"([1, 2, 3])");

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(), 3);

    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(std::get<0>(values_[i]), jsom::JsonType::Number);
        EXPECT_EQ(std::get<1>(values_[i]), std::to_string(i + 1));
        EXPECT_EQ(std::get<2>(values_[i]), "/" + std::to_string(i));
    }
}

// Test memory management with arena allocator
TEST_F(Phase7Test, ArenaAllocatorUsage) {
    constexpr int SOME_NUMBER = 1024;
    auto arena = std::make_unique<jsom::ArenaAllocator>(SOME_NUMBER);
    jsom::StreamingParser arena_parser(std::move(arena));

    std::vector<std::tuple<jsom::JsonType, std::string, std::string>> arena_values;

    jsom::ParseEvents events;
    events.on_value = [&arena_values](const jsom::JsonValue& value) {
        arena_values.emplace_back(value.type(), value.raw_value(), value.path());
    };

    arena_parser.set_events(events);
    arena_parser.parse_string(R"({"test": "arena"})");
    arena_parser.end_input();

    ASSERT_EQ(arena_values.size(), 1);
    EXPECT_EQ(std::get<1>(arena_values[0]), "arena");
    EXPECT_EQ(std::get<2>(arena_values[0]), "/test");
}

// Test complex nested structure parsing
TEST_F(Phase7Test, ComplexNestedStructure) {
    std::string complex_json = R"({
        "users": [
            {"name": "Alice", "age": 30, "active": true},
            {"name": "Bob", "age": 25, "active": false}
        ],
        "config": {
            "timeout": 5000,
            "retries": 3,
            "endpoints": ["api.example.com", "backup.example.com"]
        }
    })";

    parse(complex_json);

    EXPECT_TRUE(errors_.empty());
    ASSERT_EQ(values_.size(),
              10); // 2 names, 2 ages, 2 active flags, 1 timeout, 1 retries, 2 endpoints

    // Verify some key paths
    bool found_alice = false;
    bool found_timeout = false;
    bool found_endpoint = false;
    for (const auto& value : values_) {
        const std::string& path = std::get<2>(value);
        const std::string& val = std::get<1>(value);

        if (path == "/users/0/name" && val == "Alice") {
            found_alice = true;
	}
        if (path == "/config/timeout" && val == "5000") {
            found_timeout = true;
	}
        if (path == "/config/endpoints/0" && val == "api.example.com") {
            found_endpoint = true;
	}
    }

    EXPECT_TRUE(found_alice);
    EXPECT_TRUE(found_timeout);
    EXPECT_TRUE(found_endpoint);
}
