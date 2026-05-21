#include <catch2/catch_test_macros.hpp>
#include <string>

#include "crdt/lww_register.hpp"

using crdt::LWWMetadata;
using crdt::LWWOperation;
using crdt::LWWRegister;

TEST_CASE("Local set stores value and advances timestamp", "[lww][local]") {
    LWWRegister<std::string> title(1, "Untitled");

    const auto op = title.set("Meeting Notes");

    REQUIRE(title.value() == "Meeting Notes");
    REQUIRE(title.metadata().timestamp == 1);
    REQUIRE(title.metadata().peer_id == 1);
    REQUIRE(op.value == "Meeting Notes");
    REQUIRE(op.metadata == title.metadata());
}

TEST_CASE("Remote write with higher timestamp wins", "[lww][merge]") {
    LWWRegister<std::string> title(1, "Local");
    title.set("Local Update");  // timestamp = 1

    title.apply({"Remote Update", LWWMetadata{5, 2}});

    REQUIRE(title.value() == "Remote Update");
    REQUIRE(title.metadata() == (LWWMetadata{5, 2}));
}

TEST_CASE("Remote write with lower timestamp loses", "[lww][merge]") {
    LWWRegister<std::string> title(1, "Local");
    title.apply({"Current", LWWMetadata{5, 1}});

    title.apply({"Stale", LWWMetadata{4, 9}});

    REQUIRE(title.value() == "Current");
    REQUIRE(title.metadata() == (LWWMetadata{5, 1}));
}

TEST_CASE("Equal timestamp: higher peer_id wins", "[lww][tiebreaker]") {
    LWWRegister<std::string> title(1, "Untitled");

    title.apply({"Peer Two", LWWMetadata{5, 2}});
    title.apply({"Peer One", LWWMetadata{5, 1}});

    REQUIRE(title.value() == "Peer Two");
    REQUIRE(title.metadata() == (LWWMetadata{5, 2}));

    title.apply({"Peer Three", LWWMetadata{5, 3}});

    REQUIRE(title.value() == "Peer Three");
    REQUIRE(title.metadata() == (LWWMetadata{5, 3}));
}

TEST_CASE("Applying the same operation twice is idempotent", "[lww][idempotent]") {
    LWWRegister<std::string> title(1, "Untitled");
    const LWWOperation<std::string> op{"Remote", LWWMetadata{7, 2}};

    title.apply(op);
    title.apply(op);

    REQUIRE(title.value() == "Remote");
    REQUIRE(title.metadata() == op.metadata);
}

TEST_CASE("Replicas converge regardless of operation arrival order", "[lww][convergence]") {
    const LWWOperation<std::string> op_peer_one{"Peer One", LWWMetadata{3, 1}};
    const LWWOperation<std::string> op_peer_two{"Peer Two", LWWMetadata{3, 2}};
    const LWWOperation<std::string> op_latest  {"Latest",   LWWMetadata{4, 1}};

    LWWRegister<std::string> replica_one(1, "Untitled");
    LWWRegister<std::string> replica_two(2, "Untitled");

    replica_one.apply(op_peer_one);
    replica_one.apply(op_peer_two);
    replica_one.apply(op_latest);

    replica_two.apply(op_latest);
    replica_two.apply(op_peer_two);
    replica_two.apply(op_peer_one);

    REQUIRE(replica_one.value() == replica_two.value());
    REQUIRE(replica_one.metadata() == replica_two.metadata());
    REQUIRE(replica_one.value() == "Latest");
}

TEST_CASE("merge() is equivalent to apply() with the other register's state", "[lww][merge]") {
    LWWRegister<std::string> source(2, "Untitled");
    source.apply({"Source", LWWMetadata{10, 2}});

    LWWRegister<std::string> via_merge(1, "Local");
    LWWRegister<std::string> via_apply(1, "Local");

    via_merge.merge(source);
    via_apply.apply({"Source", LWWMetadata{10, 2}});

    REQUIRE(via_merge.value() == via_apply.value());
    REQUIRE(via_merge.metadata() == via_apply.metadata());
}

TEST_CASE("Lamport clock advances when observing a remote timestamp", "[lww][clock]") {
    LWWRegister<std::string> title(1, "Untitled");

    // Observe a remote timestamp of 10.
    title.apply({"Remote", LWWMetadata{10, 2}});

    // The next local set must produce a timestamp > 10 (clock catches up + ticks).
    const auto op = title.set("Local After Remote");
    REQUIRE(op.metadata.timestamp > 10);
}

TEST_CASE("merge() is commutative", "[lww][convergence]") {
    LWWRegister<std::string> a(1, "Untitled");
    LWWRegister<std::string> b(2, "Untitled");

    a.apply({"A write", LWWMetadata{3, 1}});
    b.apply({"B write", LWWMetadata{5, 2}});

    LWWRegister<std::string> ab(1, "Untitled");
    ab.merge(a);
    ab.merge(b);

    LWWRegister<std::string> ba(1, "Untitled");
    ba.merge(b);
    ba.merge(a);

    REQUIRE(ab.value() == ba.value());
    REQUIRE(ab.metadata() == ba.metadata());
}
