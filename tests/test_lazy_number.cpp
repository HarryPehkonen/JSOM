#include <gtest/gtest.h>
#include <jsom/core_types.hpp>

using namespace jsom;

TEST(LazyNumberTest, ConstructFromString) {
    LazyNumber num("42");
    EXPECT_EQ(num.as_int(), 42);
    EXPECT_EQ(num.as_double(), 42.0);
    EXPECT_EQ(num.as_string(), "42");
}

TEST(LazyNumberTest, ConstructFromDouble) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    LazyNumber num(42.5);
    EXPECT_EQ(num.as_double(), 42.5);
    EXPECT_EQ(num.as_string(), "42.5");
}

TEST(LazyNumberTest, ConstructFromInt) {
    // NOLINTNEXTLINE(readability-magic-numbers)
    LazyNumber num(42);
    EXPECT_EQ(num.as_int(), 42);
    EXPECT_EQ(num.as_double(), 42.0);
}

TEST(LazyNumberTest, FormatPreservation) {
    LazyNumber num1("1.0");
    EXPECT_EQ(num1.as_string(), "1.0");

    LazyNumber num2("1e10");
    EXPECT_EQ(num2.as_string(), "1e10");
    EXPECT_EQ(num2.as_double(), 1e10);
}

TEST(LazyNumberTest, InvalidConversion) {
    LazyNumber num("not_a_number");
    EXPECT_THROW(num.as_double(), TypeException);
    EXPECT_THROW(num.as_int(), TypeException);
}

TEST(LazyNumberTest, IntegerCheck) {
    LazyNumber int_num("42");
    EXPECT_TRUE(int_num.is_integer());

    LazyNumber float_num("42.5");
    EXPECT_FALSE(float_num.is_integer());
}

TEST(LazyNumberTest, Equality) {
    LazyNumber num1("42");
    // NOLINTNEXTLINE(readability-magic-numbers)
    LazyNumber num2(42.0);
    EXPECT_TRUE(num1 == num2);
}

TEST(LazyNumberTest, Serialization) {
    std::ostringstream oss;
    LazyNumber num("1.0");
    num.serialize(oss);
    EXPECT_EQ(oss.str(), "1.0");
}
