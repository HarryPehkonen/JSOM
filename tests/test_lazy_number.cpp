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

TEST(LazyNumberTest, IntConversionOutOfRangeThrows) {
    // Regression: as_int() on a value outside int's range used to narrow-cast
    // BEFORE the range check — undefined behavior. Caught by UBSan during
    // fuzzing: "8.88889e+15 is outside the range of representable values of
    // type 'int'".
    LazyNumber huge("8888888888888888");  // 8.88e15 > INT_MAX
    EXPECT_THROW((void)huge.as_int(), TypeException);

    LazyNumber tiny("-8888888888888888");  // < INT_MIN
    EXPECT_THROW((void)tiny.as_int(), TypeException);
}

TEST(LazyNumberTest, InvalidConversion) {
    LazyNumber num("not_a_number");
    // as_double()/as_int() are [[nodiscard]] and throw on invalid input;
    // EXPECT_THROW discards the return value, so cast to void to silence
    // -Wunused-result (the call still happens and the throw is caught).
    EXPECT_THROW((void)num.as_double(), TypeException);
    EXPECT_THROW((void)num.as_int(), TypeException);
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
