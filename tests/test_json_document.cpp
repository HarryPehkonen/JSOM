#include <gtest/gtest.h>
#include <jsom/json_document.hpp>

using namespace jsom;

constexpr int TEST_PERSON_AGE = 30;

TEST(JsonDocumentTest, ConstructNull) {
    JsonDocument doc;
    EXPECT_TRUE(doc.is_null());
    EXPECT_EQ(doc.type(), JsonType::Null);
}

TEST(JsonDocumentTest, ConstructBoolean) {
    JsonDocument doc(true);
    EXPECT_TRUE(doc.is_bool());
    EXPECT_EQ(doc.as<bool>(), true);
}

TEST(JsonDocumentTest, ConstructNumber) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument doc(42);
    EXPECT_TRUE(doc.is_number());
    EXPECT_EQ(doc.as<int>(), 42);
    EXPECT_EQ(doc.as<double>(), 42.0);
}

TEST(JsonDocumentTest, ConstructString) {
    JsonDocument doc("hello");
    EXPECT_TRUE(doc.is_string());
    EXPECT_EQ(doc.as<std::string>(), "hello");
}

TEST(JsonDocumentTest, ConstructObject) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument doc{{"name", "John"}, {"age", 30}};
    EXPECT_TRUE(doc.is_object());
    EXPECT_EQ(doc["name"].as<std::string>(), "John");
    EXPECT_EQ(doc["age"].as<int>(), 30);
}

TEST(JsonDocumentTest, ConstructArray) {
    auto doc = JsonDocument(std::vector<JsonDocument>{1, 2, 3});
    EXPECT_TRUE(doc.is_array());
    EXPECT_EQ(doc[0].as<int>(), 1);
    EXPECT_EQ(doc[1].as<int>(), 2);
    EXPECT_EQ(doc[2].as<int>(), 3);
}

TEST(JsonDocumentTest, TypeValidation) {
    JsonDocument doc("hello");
    EXPECT_THROW(doc.as<int>(), TypeException);
    EXPECT_THROW(doc["key"], TypeException);
    EXPECT_THROW(doc[0], TypeException);
}

TEST(JsonDocumentTest, OptionalAccess) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument doc(42);
    auto maybe_int = doc.try_as<int>();
    EXPECT_TRUE(maybe_int.has_value());
    if (maybe_int.has_value()) {
        EXPECT_EQ(*maybe_int, 42);
    }

    auto maybe_string = doc.try_as<std::string>();
    EXPECT_FALSE(maybe_string.has_value());
}

TEST(JsonDocumentTest, Serialization) {
    JsonDocument doc{{"name", "John"},
                     {"age", TEST_PERSON_AGE},
                     {"active", true}};

    std::string json = doc.to_json();
    EXPECT_TRUE(json.find("\"name\":\"John\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"age\":30") != std::string::npos);
    EXPECT_TRUE(json.find("\"active\":true") != std::string::npos);
}

TEST(JsonDocumentTest, LazyNumberCreation) {
    JsonDocument doc = JsonDocument::from_lazy_number("1.0");
    EXPECT_TRUE(doc.is_number());
    EXPECT_EQ(doc.as<double>(), 1.0);

    std::string serialized = doc.to_json();
    EXPECT_EQ(serialized, "1.0");
}

TEST(JsonDocumentTest, AsArray) {
    auto doc = JsonDocument(std::vector<JsonDocument>{1, 2, 3});
    const auto& arr = doc.as_array();
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0].as<int>(), 1);
    EXPECT_EQ(arr[2].as<int>(), 3);

    // Wrong type throws
    JsonDocument str("hello");
    EXPECT_THROW(str.as_array(), TypeException);
}

TEST(JsonDocumentTest, AsObject) {
    JsonDocument doc{{"x", 10}, {"y", 20}};
    const auto& obj = doc.as_object();
    EXPECT_EQ(obj.size(), 2);
    EXPECT_EQ(obj.at("x").as<int>(), 10);

    // Wrong type throws
    JsonDocument num(42);
    EXPECT_THROW(num.as_object(), TypeException);
}

TEST(JsonDocumentTest, Size) {
    auto arr = JsonDocument(std::vector<JsonDocument>{1, 2});
    EXPECT_EQ(arr.size(), 2);

    JsonDocument obj{{"a", 1}, {"b", 2}, {"c", 3}};
    EXPECT_EQ(obj.size(), 3);

    // Primitives throw
    JsonDocument num(42);
    EXPECT_THROW(num.size(), TypeException);
}

TEST(JsonDocumentTest, Empty) {
    // Null is empty
    JsonDocument null_doc;
    EXPECT_TRUE(null_doc.empty());

    // Empty containers
    auto empty_arr = JsonDocument::make_array();
    EXPECT_TRUE(empty_arr.empty());
    auto empty_obj = JsonDocument::make_object();
    EXPECT_TRUE(empty_obj.empty());

    // Non-empty containers
    auto arr = JsonDocument(std::vector<JsonDocument>{1});
    EXPECT_FALSE(arr.empty());
    JsonDocument obj{{"k", 1}};
    EXPECT_FALSE(obj.empty());

    // Primitives throw
    JsonDocument num(42);
    EXPECT_THROW(num.empty(), TypeException);
}

TEST(JsonDocumentTest, Contains) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    JsonDocument doc{{"name", "Alice"}, {"age", 30}};
    EXPECT_TRUE(doc.contains("name"));
    EXPECT_TRUE(doc.contains("age"));
    EXPECT_FALSE(doc.contains("email"));

    // Non-object throws
    auto arr = JsonDocument(std::vector<JsonDocument>{1});
    EXPECT_THROW(arr.contains("x"), TypeException);
}

TEST(JsonDocumentTest, PushBack) {
    auto doc = JsonDocument::make_array();
    doc.push_back(JsonDocument(1));
    doc.push_back(JsonDocument("two"));
    doc.push_back(JsonDocument(3.0));

    EXPECT_EQ(doc.size(), 3);
    EXPECT_EQ(doc[0].as<int>(), 1);
    EXPECT_EQ(doc[1].as<std::string>(), "two");
    EXPECT_EQ(doc[2].as<double>(), 3.0);

    // Non-array throws
    JsonDocument obj{{"k", 1}};
    EXPECT_THROW(obj.push_back(1), TypeException);
}

TEST(JsonDocumentTest, MakeArray) {
    auto arr = JsonDocument::make_array();
    EXPECT_TRUE(arr.is_array());
    EXPECT_TRUE(arr.empty());
    EXPECT_EQ(arr.size(), 0);
}

TEST(JsonDocumentTest, MakeObject) {
    auto obj = JsonDocument::make_object();
    EXPECT_TRUE(obj.is_object());
    EXPECT_TRUE(obj.empty());
    EXPECT_EQ(obj.size(), 0);
}

// ============================================================
// Implicit Construction Tests (Feature #6)
// ============================================================

TEST(ImplicitConstructionTest, FromBool) {
    JsonDocument doc = true;
    EXPECT_TRUE(doc.is_bool());
    EXPECT_EQ(doc.as<bool>(), true);

    JsonDocument doc2 = false;
    EXPECT_EQ(doc2.as<bool>(), false);
}

TEST(ImplicitConstructionTest, FromInt) {
    JsonDocument doc = 42;
    EXPECT_TRUE(doc.is_number());
    EXPECT_EQ(doc.as<int>(), 42);
}

TEST(ImplicitConstructionTest, FromDouble) {
    JsonDocument doc = 3.14;
    EXPECT_TRUE(doc.is_number());
    EXPECT_DOUBLE_EQ(doc.as<double>(), 3.14);
}

TEST(ImplicitConstructionTest, FromString) {
    std::string s = "hello";
    JsonDocument doc = s;
    EXPECT_TRUE(doc.is_string());
    EXPECT_EQ(doc.as<std::string>(), "hello");
}

TEST(ImplicitConstructionTest, FromConstChar) {
    JsonDocument doc = "world";
    EXPECT_TRUE(doc.is_string());
    EXPECT_EQ(doc.as<std::string>(), "world");
}

TEST(ImplicitConstructionTest, FromNullptr) {
    JsonDocument doc = nullptr;
    EXPECT_TRUE(doc.is_null());
}

TEST(ImplicitConstructionTest, AssignmentSyntax) {
    // Verify assignment-style construction works for all types
    JsonDocument a = 1;
    JsonDocument b = 2.5;
    JsonDocument c = true;
    JsonDocument d = "text";
    JsonDocument e = nullptr;

    EXPECT_EQ(a.as<int>(), 1);
    EXPECT_DOUBLE_EQ(b.as<double>(), 2.5);
    EXPECT_EQ(c.as<bool>(), true);
    EXPECT_EQ(d.as<std::string>(), "text");
    EXPECT_TRUE(e.is_null());
}

TEST(ImplicitConstructionTest, ImplicitInContainers) {
    // Implicit conversion enables clean initializer_list construction
    JsonDocument obj{{"name", "Alice"}, {"age", 30}, {"active", true}};
    EXPECT_EQ(obj["name"].as<std::string>(), "Alice");
    EXPECT_EQ(obj["age"].as<int>(), 30);
    EXPECT_EQ(obj["active"].as<bool>(), true);
}

TEST(ImplicitConstructionTest, ImplicitInPushBack) {
    auto arr = JsonDocument::make_array();
    arr.push_back(1);
    arr.push_back("hello");
    arr.push_back(true);
    arr.push_back(3.14);

    EXPECT_EQ(arr.size(), 4);
    EXPECT_EQ(arr[0].as<int>(), 1);
    EXPECT_EQ(arr[1].as<std::string>(), "hello");
    EXPECT_EQ(arr[2].as<bool>(), true);
    EXPECT_DOUBLE_EQ(arr[3].as<double>(), 3.14);
}

TEST(ImplicitConstructionTest, ImplicitInSet) {
    auto obj = JsonDocument::make_object();
    obj.set("name", "Bob");
    obj.set("age", 25);
    obj.set("score", 9.5);

    EXPECT_EQ(obj["name"].as<std::string>(), "Bob");
    EXPECT_EQ(obj["age"].as<int>(), 25);
    EXPECT_DOUBLE_EQ(obj["score"].as<double>(), 9.5);
}
