#include <catch2/catch_test_macros.hpp>

#include "crdt/GCounter.hpp"

using crdt::GCounter;

TEST_CASE("Increment increases value by 1 by default", "[gcounter][basic]") {
    GCounter c;
    c.increment("peer_1");

    REQUIRE(c.value() == 1);
}

TEST_CASE("Increment with custom amount increases value correctly", "[gcounter][basic]") {
    GCounter c;
    c.increment("peer_1", 5);
    c.increment("peer_1", 3);

    REQUIRE(c.value() == 8);
}

TEST_CASE("Value is the sum across all peers", "[gcounter][basic]") {
    GCounter c;
    c.increment("peer_1", 4);
    c.increment("peer_2", 6);
    c.increment("peer_3", 2);

    REQUIRE(c.value() == 12);
}

TEST_CASE("Merge with disjoint peers sums all counts", "[gcounter][merge]") {
    GCounter a;
    a.increment("peer_1", 3);

    GCounter b;
    b.increment("peer_2", 7);

    a.merge(b);

    REQUIRE(a.value() == 10);
}

TEST_CASE("Merge with overlapping peers takes max per peer", "[gcounter][merge]") {
    GCounter a;
    a.increment("peer_1", 10);
    a.increment("peer_2", 2);

    GCounter b;
    b.increment("peer_1", 5);   // lower — should not overwrite
    b.increment("peer_2", 8);   // higher — should win

    a.merge(b);

    REQUIRE(a.value() == 18);  // max(10,5) + max(2,8) = 10 + 8
}

TEST_CASE("Merge is commutative", "[gcounter][convergence]") {
    GCounter a;
    a.increment("peer_1", 4);

    GCounter b;
    b.increment("peer_2", 9);

    GCounter ab;
    ab.merge(a);
    ab.merge(b);

    GCounter ba;
    ba.merge(b);
    ba.merge(a);

    REQUIRE(ab.value() == ba.value());
}

TEST_CASE("Merge is idempotent", "[gcounter][idempotent]") {
    GCounter a;
    a.increment("peer_1", 5);

    GCounter b;
    b.increment("peer_1", 5);

    a.merge(b);
    a.merge(b);  // second merge must not double-count

    REQUIRE(a.value() == 5);
}

TEST_CASE("Merge is associative", "[gcounter][convergence]") {
    GCounter a;
    a.increment("peer_1", 2);

    GCounter b;
    b.increment("peer_2", 3);

    GCounter c;
    c.increment("peer_3", 4);

    // (a merge b) merge c
    GCounter abc;
    abc.merge(a);
    abc.merge(b);
    abc.merge(c);

    // a merge (b merge c)
    GCounter bc;
    bc.merge(b);
    bc.merge(c);

    GCounter a_bc;
    a_bc.merge(a);
    a_bc.merge(bc);

    REQUIRE(abc.value() == a_bc.value());
}

TEST_CASE("Fresh counter has value zero", "[gcounter][basic]") {
    GCounter c;
    REQUIRE(c.value() == 0);
}

TEST_CASE("Each peer's increments are tracked independently", "[gcounter][basic]") {
    GCounter c;
    c.increment("peer_1", 3);
    c.increment("peer_2", 3);

    // Merging a counter where peer_1 = 3 should not change anything.
    GCounter other;
    other.increment("peer_1", 3);
    c.merge(other);

    REQUIRE(c.value() == 6);  // peer_2's count must be preserved
}
