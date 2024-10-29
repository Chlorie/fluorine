#include <catch2/catch_test_macros.hpp>

TEST_CASE("example test case", "[example]")
{
    SECTION("math")
    {
        CHECK(1 + 1 == 2);
    }
}
