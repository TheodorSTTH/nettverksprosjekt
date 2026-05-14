#include <catch2/catch_test_macros.hpp>

#include "crdt/GCounter.hpp"

TEST_CASE("GCounter sums peer components", "[gcounter]") {
    crdt::GCounter counter;

    counter.merge_count("1", 3);
    counter.merge_count("2", 4);

    REQUIRE(counter.count_for("1") == 3);
    REQUIRE(counter.count_for("2") == 4);
    REQUIRE(counter.value() == 7);
}

TEST_CASE("GCounter merge keeps maximum component counts", "[gcounter]") {
    crdt::GCounter left;
    crdt::GCounter right;

    left.merge_count("1", 5);
    left.merge_count("2", 1);
    right.merge_count("1", 3);
    right.merge_count("2", 7);

    left.merge(right);

    REQUIRE(left.count_for("1") == 5);
    REQUIRE(left.count_for("2") == 7);
    REQUIRE(left.value() == 12);
}

TEST_CASE("GCounter merge_count ignores stale component counts", "[gcounter]") {
    crdt::GCounter counter;

    REQUIRE(counter.merge_count("1", 5));
    REQUIRE_FALSE(counter.merge_count("1", 5));
    REQUIRE_FALSE(counter.merge_count("1", 4));
    REQUIRE(counter.count_for("1") == 5);
}
