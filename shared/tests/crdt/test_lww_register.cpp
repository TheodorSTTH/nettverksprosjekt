#include <string>

#include <catch2/catch_test_macros.hpp>

#include "crdt/lww_register.hpp"

namespace {

using crdt::LWWMetadata;
using crdt::LWWOperation;
using crdt::LWWRegister;

TEST_CASE("LWW register local set stores value and timestamp", "[lww_register]") {
    LWWRegister<std::string> title(1, "Untitled");

    const auto operation = title.set("Meeting Notes");

    REQUIRE(title.value() == "Meeting Notes");
    REQUIRE(title.metadata().timestamp == 1);
    REQUIRE(title.metadata().peer_id == 1);
    REQUIRE(operation.value == "Meeting Notes");
    REQUIRE(operation.metadata == title.metadata());
}

TEST_CASE("LWW register higher timestamp remote operation wins", "[lww_register]") {
    LWWRegister<std::string> title(1, "Local");
    title.set("Local Update");

    title.apply({"Remote Update", LWWMetadata{5, 2}});

    REQUIRE(title.value() == "Remote Update");
    REQUIRE(title.metadata() == (LWWMetadata{5, 2}));
}

TEST_CASE("LWW register lower timestamp remote operation loses", "[lww_register]") {
    LWWRegister<std::string> title(1, "Local");
    title.apply({"Current", LWWMetadata{5, 1}});

    title.apply({"Stale", LWWMetadata{4, 9}});

    REQUIRE(title.value() == "Current");
    REQUIRE(title.metadata() == (LWWMetadata{5, 1}));
}

TEST_CASE("LWW register equal timestamp uses peer id tiebreaker", "[lww_register]") {
    LWWRegister<std::string> title(1, "Local");
    title.apply({"Peer Two", LWWMetadata{5, 2}});
    title.apply({"Peer One", LWWMetadata{5, 1}});

    REQUIRE(title.value() == "Peer Two");
    REQUIRE(title.metadata() == (LWWMetadata{5, 2}));

    title.apply({"Peer Three", LWWMetadata{5, 3}});

    REQUIRE(title.value() == "Peer Three");
    REQUIRE(title.metadata() == (LWWMetadata{5, 3}));
}

TEST_CASE("LWW register same operation is idempotent", "[lww_register]") {
    LWWRegister<std::string> title(1, "Untitled");
    const LWWOperation<std::string> operation{"Remote", LWWMetadata{7, 2}};

    title.apply(operation);
    title.apply(operation);

    REQUIRE(title.value() == "Remote");
    REQUIRE(title.metadata() == operation.metadata);
}

TEST_CASE("LWW register replicas converge with different operation orders", "[lww_register]") {
    const LWWOperation<std::string> peer_one_update{"Peer One", LWWMetadata{3, 1}};
    const LWWOperation<std::string> peer_two_update{"Peer Two", LWWMetadata{3, 2}};
    const LWWOperation<std::string> latest_update{"Latest", LWWMetadata{4, 1}};

    LWWRegister<std::string> replica_one(1, "Untitled");
    LWWRegister<std::string> replica_two(2, "Untitled");

    replica_one.apply(peer_one_update);
    replica_one.apply(peer_two_update);
    replica_one.apply(latest_update);

    replica_two.apply(latest_update);
    replica_two.apply(peer_two_update);
    replica_two.apply(peer_one_update);

    REQUIRE(replica_one.value() == replica_two.value());
    REQUIRE(replica_one.metadata() == replica_two.metadata());
    REQUIRE(replica_one.value() == "Latest");
}

TEST_CASE("LWW register merge matches applying other register state", "[lww_register]") {
    LWWRegister<std::string> source(2, "Untitled");
    source.apply({"Source", LWWMetadata{10, 2}});

    LWWRegister<std::string> merged(1, "Local");
    LWWRegister<std::string> applied(1, "Local");

    merged.merge(source);
    applied.apply({"Source", LWWMetadata{10, 2}});

    REQUIRE(merged.value() == applied.value());
    REQUIRE(merged.metadata() == applied.metadata());
}

}  // namespace
