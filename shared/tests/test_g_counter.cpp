#include <catch2/catch_test_macros.hpp>

#include "crdt/GCounter.hpp"

using crdt::GCounter;

// merge_count(node_id, count) sets counts_[node_id] = max(current, count),
// returning true if the value changed. Clients track their own absolute
// count and broadcast it; the server stores the max-per-peer.

TEST_CASE("merge_count stores a fresh peer count", "[gcounter][basic]") {
    GCounter c;
    REQUIRE(c.merge_count("peer_1", 1) == true);
    REQUIRE(c.value() == 1);
    REQUIRE(c.count_for("peer_1") == 1);
}

TEST_CASE("merge_count with higher count overwrites", "[gcounter][basic]") {
    GCounter c;
    c.merge_count("peer_1", 5);
    REQUIRE(c.merge_count("peer_1", 8) == true);
    REQUIRE(c.count_for("peer_1") == 8);
}

TEST_CASE("merge_count with lower count is a no-op", "[gcounter][basic]") {
    GCounter c;
    c.merge_count("peer_1", 8);
    REQUIRE(c.merge_count("peer_1", 3) == false);
    REQUIRE(c.count_for("peer_1") == 8);
}

TEST_CASE("merge_count with equal count is a no-op", "[gcounter][idempotent]") {
    GCounter c;
    c.merge_count("peer_1", 5);
    REQUIRE(c.merge_count("peer_1", 5) == false);
    REQUIRE(c.value() == 5);
}

TEST_CASE("Value is the sum across all peers", "[gcounter][basic]") {
    GCounter c;
    c.merge_count("peer_1", 4);
    c.merge_count("peer_2", 6);
    c.merge_count("peer_3", 2);

    REQUIRE(c.value() == 12);
}

TEST_CASE("Merge with disjoint peers sums all counts", "[gcounter][merge]") {
    GCounter a;
    a.merge_count("peer_1", 3);

    GCounter b;
    b.merge_count("peer_2", 7);

    a.merge(b);

    REQUIRE(a.value() == 10);
    REQUIRE(a.count_for("peer_1") == 3);
    REQUIRE(a.count_for("peer_2") == 7);
}

TEST_CASE("Merge with overlapping peers takes max per peer", "[gcounter][merge]") {
    GCounter a;
    a.merge_count("peer_1", 10);
    a.merge_count("peer_2", 2);

    GCounter b;
    b.merge_count("peer_1", 5);   // lower — should not overwrite
    b.merge_count("peer_2", 8);   // higher — should win

    a.merge(b);

    REQUIRE(a.count_for("peer_1") == 10);
    REQUIRE(a.count_for("peer_2") == 8);
    REQUIRE(a.value() == 18);
}

TEST_CASE("Merge is commutative", "[gcounter][convergence]") {
    GCounter a;
    a.merge_count("peer_1", 4);

    GCounter b;
    b.merge_count("peer_2", 9);

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
    a.merge_count("peer_1", 5);

    GCounter b;
    b.merge_count("peer_1", 5);

    a.merge(b);
    a.merge(b);  // second merge must not double-count

    REQUIRE(a.value() == 5);
}

TEST_CASE("Merge is associative", "[gcounter][convergence]") {
    GCounter a;
    a.merge_count("peer_1", 2);

    GCounter b;
    b.merge_count("peer_2", 3);

    GCounter c;
    c.merge_count("peer_3", 4);

    GCounter abc;
    abc.merge(a);
    abc.merge(b);
    abc.merge(c);

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
    REQUIRE(c.count_for("peer_1") == 0);
}

TEST_CASE("Each peer's counts are tracked independently", "[gcounter][basic]") {
    GCounter c;
    c.merge_count("peer_1", 3);
    c.merge_count("peer_2", 3);

    // Re-merging peer_1's value should not affect peer_2.
    GCounter other;
    other.merge_count("peer_1", 3);
    c.merge(other);

    REQUIRE(c.count_for("peer_1") == 3);
    REQUIRE(c.count_for("peer_2") == 3);
    REQUIRE(c.value() == 6);
}

TEST_CASE("counts() exposes the full per-peer map", "[gcounter][basic]") {
    GCounter c;
    c.merge_count("peer_1", 4);
    c.merge_count("peer_2", 7);

    const auto& counts = c.counts();
    REQUIRE(counts.size() == 2);
    REQUIRE(counts.at("peer_1") == 4);
    REQUIRE(counts.at("peer_2") == 7);
}
