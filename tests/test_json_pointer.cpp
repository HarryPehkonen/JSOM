#include "jsom/fast_parser.hpp"
#include "jsom/json_document.hpp"
#include "jsom/json_pointer.hpp"
#include <gtest/gtest.h>

using namespace jsom;

class JsonPointerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string json = R"({
            "users": [
                {
                    "name": "Alice",
                    "age": 30,
                    "profile": {
                        "email": "alice@example.com",
                        "active": true
                    }
                },
                {
                    "name": "Bob", 
                    "age": 25,
                    "profile": {
                        "email": "bob@example.com",
                        "active": false
                    }
                }
            ],
            "config": {
                "database": {
                    "host": "localhost",
                    "port": 5432
                },
                "cache": {
                    "ttl": 3600
                }
            }
        })";

        doc = FastParser().parse(json);
    }

    JsonDocument doc;
};

TEST_F(JsonPointerTest, BasicNavigation) {
    // Test basic path navigation
    EXPECT_EQ(doc.at("/users/0/name").as<std::string>(), "Alice");
    EXPECT_EQ(doc.at("/users/0/age").as<int>(), 30);
    EXPECT_EQ(doc.at("/users/1/name").as<std::string>(), "Bob");
    EXPECT_EQ(doc.at("/users/1/age").as<int>(), 25);
}

TEST_F(JsonPointerTest, NestedObjectNavigation) {
    // Test nested object access
    EXPECT_EQ(doc.at("/users/0/profile/email").as<std::string>(), "alice@example.com");
    EXPECT_EQ(doc.at("/users/0/profile/active").as<bool>(), true);
    EXPECT_EQ(doc.at("/users/1/profile/active").as<bool>(), false);

    EXPECT_EQ(doc.at("/config/database/host").as<std::string>(), "localhost");
    EXPECT_EQ(doc.at("/config/database/port").as<int>(), 5432);
    EXPECT_EQ(doc.at("/config/cache/ttl").as<int>(), 3600);
}

TEST_F(JsonPointerTest, ExistsCheck) {
    // Test path existence checking
    EXPECT_TRUE(doc.exists("/users"));
    EXPECT_TRUE(doc.exists("/users/0"));
    EXPECT_TRUE(doc.exists("/users/0/name"));
    EXPECT_TRUE(doc.exists("/config/database/host"));

    EXPECT_FALSE(doc.exists("/users/2"));
    EXPECT_FALSE(doc.exists("/users/0/nonexistent"));
    EXPECT_FALSE(doc.exists("/invalid"));
}

TEST_F(JsonPointerTest, FindOperation) {
    // Test safe navigation with find
    auto* alice_name = doc.find("/users/0/name");
    ASSERT_NE(alice_name, nullptr);
    EXPECT_EQ(alice_name->as<std::string>(), "Alice");

    auto* invalid_path = doc.find("/users/10/name");
    EXPECT_EQ(invalid_path, nullptr);

    auto* host = doc.find("/config/database/host");
    ASSERT_NE(host, nullptr);
    EXPECT_EQ(host->as<std::string>(), "localhost");
}

TEST_F(JsonPointerTest, ErrorHandling) {
    // Test error handling for invalid paths
    EXPECT_THROW(doc.at("/users/10/name"), JsonPointerNotFoundException);
    EXPECT_THROW(doc.at("/nonexistent"), JsonPointerNotFoundException);
    EXPECT_THROW(doc.at("/users/0/name/invalid"), JsonPointerNotFoundException);
}

TEST_F(JsonPointerTest, RootAccess) {
    // Test root access (empty pointer)
    const auto& root = doc.at("");
    EXPECT_TRUE(root.is_object());
    EXPECT_TRUE(root.exists("/users"));
}

TEST_F(JsonPointerTest, BulkOperations) {
    // Test bulk path operations
    std::vector<std::string> paths
        = {"/users/0/name", "/users/0/age", "/users/1/name", "/config/database/host"};

    auto results = doc.at_multiple(paths);
    ASSERT_EQ(results.size(), 4);

    EXPECT_EQ(results[0]->as<std::string>(), "Alice");
    EXPECT_EQ(results[1]->as<int>(), 30);
    EXPECT_EQ(results[2]->as<std::string>(), "Bob");
    EXPECT_EQ(results[3]->as<std::string>(), "localhost");

    // Test bulk existence checking
    auto exist_results = doc.exists_multiple(paths);
    ASSERT_EQ(exist_results.size(), 4);
    for (bool exists : exist_results) {
        EXPECT_TRUE(exists);
    }
}

TEST_F(JsonPointerTest, PathEnumeration) {
    // Test path enumeration
    auto paths = doc.list_paths(2); // Max depth 2

    // Should contain various paths
    auto has_path = [&paths](const std::string& path) {
        return std::find(paths.begin(), paths.end(), path) != paths.end();
    };

    EXPECT_TRUE(has_path("")); // Root
    EXPECT_TRUE(has_path("/users"));
    EXPECT_TRUE(has_path("/config"));
    EXPECT_TRUE(has_path("/users/0"));
    EXPECT_TRUE(has_path("/users/1"));

    // Should not contain deeper paths due to depth limit
    // (This depends on the exact implementation of enumerate_paths)
}

TEST_F(JsonPointerTest, PathModification) {
    // Test path-based modification
    doc.set_at("/config/database/host", JsonDocument("newhost"));
    EXPECT_EQ(doc.at("/config/database/host").as<std::string>(), "newhost");

    // NOLINTNEXTLINE(readability-magic-numbers)
    doc.set_at("/config/new_setting", JsonDocument(42));
    EXPECT_EQ(doc.at("/config/new_setting").as<int>(), 42);
}

TEST_F(JsonPointerTest, PathRemoval) {
    // Create a fresh document for this test to avoid interference from previous tests
    std::string fresh_json = R"({
            "config": {
                "cache": {
                    "ttl": 3600
                }
            }
        })";
    auto fresh_doc = FastParser().parse(fresh_json);

    // Test path-based removal
    EXPECT_TRUE(fresh_doc.exists("/config/cache/ttl"));
    EXPECT_TRUE(fresh_doc.remove_at("/config/cache/ttl"));
    EXPECT_FALSE(fresh_doc.exists("/config/cache/ttl"));

    // Removing non-existent path should return false
    EXPECT_FALSE(fresh_doc.remove_at("/nonexistent"));
}

// Test JSON Pointer utility functions
TEST(JsonPointerUtilTest, Parsing) {
    // Test pointer parsing
    auto segments = JsonPointer::parse("/users/0/name");
    ASSERT_EQ(segments.size(), 3);
    EXPECT_EQ(segments[0], "users");
    EXPECT_EQ(segments[1], "0");
    EXPECT_EQ(segments[2], "name");

    // Test empty pointer
    auto empty = JsonPointer::parse("");
    EXPECT_TRUE(empty.empty());
}

TEST(JsonPointerUtilTest, Building) {
    // Test pointer building
    std::vector<std::string> segments = {"users", "0", "name"};
    auto pointer = JsonPointer::build(segments);
    EXPECT_EQ(pointer, "/users/0/name");

    // Test empty segments
    auto empty_pointer = JsonPointer::build({});
    EXPECT_EQ(empty_pointer, "");
}

TEST(JsonPointerUtilTest, Escaping) {
    // Test segment escaping
    EXPECT_EQ(JsonPointer::escape_segment("test"), "test");
    EXPECT_EQ(JsonPointer::escape_segment("test/path"), "test~1path");
    EXPECT_EQ(JsonPointer::escape_segment("test~value"), "test~0value");
    EXPECT_EQ(JsonPointer::escape_segment("test~/path"), "test~0~1path");

    // Test segment unescaping
    EXPECT_EQ(JsonPointer::unescape_segment("test"), "test");
    EXPECT_EQ(JsonPointer::unescape_segment("test~1path"), "test/path");
    EXPECT_EQ(JsonPointer::unescape_segment("test~0value"), "test~value");
    EXPECT_EQ(JsonPointer::unescape_segment("test~0~1path"), "test~/path");
}

TEST(JsonPointerUtilTest, Validation) {
    // Test pointer validation
    EXPECT_TRUE(JsonPointer::is_valid("/users/0/name"));
    EXPECT_TRUE(JsonPointer::is_valid(""));
    EXPECT_TRUE(JsonPointer::is_valid("/"));

    EXPECT_FALSE(JsonPointer::is_valid("users/0/name")); // Must start with /
}

TEST(JsonPointerUtilTest, PrefixDetection) {
    // Test prefix detection
    EXPECT_TRUE(JsonPointer::is_prefix("", "/users/0/name"));
    EXPECT_TRUE(JsonPointer::is_prefix("/users", "/users/0/name"));
    EXPECT_TRUE(JsonPointer::is_prefix("/users/0", "/users/0/name"));
    EXPECT_TRUE(JsonPointer::is_prefix("/users/0/name", "/users/0/name"));

    EXPECT_FALSE(JsonPointer::is_prefix("/users/0/name", "/users/0"));
    EXPECT_FALSE(JsonPointer::is_prefix("/config", "/users/0/name"));
}

TEST(JsonPointerUtilTest, RelativePointers) {
    // Test making relative pointers
    EXPECT_EQ(JsonPointer::make_relative("/users", "/users/0/name"), "/0/name");
    EXPECT_EQ(JsonPointer::make_relative("/users/0", "/users/0/name"), "/name");
    EXPECT_EQ(JsonPointer::make_relative("", "/users/0/name"), "/users/0/name");
    EXPECT_EQ(JsonPointer::make_relative("/users/0/name", "/users/0/name"), "");
}
