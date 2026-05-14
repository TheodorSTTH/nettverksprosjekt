#include <cassert>
#include <iostream>
#include <string>

#include "crdt/lww_register.hpp"

namespace {

using crdt::LWWMetadata;
using crdt::LWWOperation;
using crdt::LWWRegister;

void test_local_set_stores_value_and_timestamp() {
    LWWRegister<std::string> title(1, "Untitled");

    const auto operation = title.set("Meeting Notes");

    assert(title.value() == "Meeting Notes");
    assert(title.metadata().timestamp == 1);
    assert(title.metadata().peer_id == 1);
    assert(operation.value == "Meeting Notes");
    assert(operation.metadata == title.metadata());
}

void test_higher_timestamp_remote_wins() {
    LWWRegister<std::string> title(1, "Local");
    title.set("Local Update");

    title.apply({"Remote Update", LWWMetadata{5, 2}});

    assert(title.value() == "Remote Update");
    assert(title.metadata() == (LWWMetadata{5, 2}));
}

void test_lower_timestamp_remote_loses() {
    LWWRegister<std::string> title(1, "Local");
    title.apply({"Current", LWWMetadata{5, 1}});

    title.apply({"Stale", LWWMetadata{4, 9}});

    assert(title.value() == "Current");
    assert(title.metadata() == (LWWMetadata{5, 1}));
}

void test_equal_timestamp_uses_peer_id_tiebreaker() {
    LWWRegister<std::string> title(1, "Local");
    title.apply({"Peer Two", LWWMetadata{5, 2}});
    title.apply({"Peer One", LWWMetadata{5, 1}});

    assert(title.value() == "Peer Two");
    assert(title.metadata() == (LWWMetadata{5, 2}));

    title.apply({"Peer Three", LWWMetadata{5, 3}});

    assert(title.value() == "Peer Three");
    assert(title.metadata() == (LWWMetadata{5, 3}));
}

void test_same_operation_is_idempotent() {
    LWWRegister<std::string> title(1, "Untitled");
    const LWWOperation<std::string> operation{"Remote", LWWMetadata{7, 2}};

    title.apply(operation);
    title.apply(operation);

    assert(title.value() == "Remote");
    assert(title.metadata() == operation.metadata);
}

void test_replicas_converge_with_different_operation_orders() {
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

    assert(replica_one.value() == replica_two.value());
    assert(replica_one.metadata() == replica_two.metadata());
    assert(replica_one.value() == "Latest");
}

void test_merge_matches_applying_other_register_state() {
    LWWRegister<std::string> source(2, "Untitled");
    source.apply({"Source", LWWMetadata{10, 2}});

    LWWRegister<std::string> merged(1, "Local");
    LWWRegister<std::string> applied(1, "Local");

    merged.merge(source);
    applied.apply({"Source", LWWMetadata{10, 2}});

    assert(merged.value() == applied.value());
    assert(merged.metadata() == applied.metadata());
}

}  // namespace

int main() {
    test_local_set_stores_value_and_timestamp();
    test_higher_timestamp_remote_wins();
    test_lower_timestamp_remote_loses();
    test_equal_timestamp_uses_peer_id_tiebreaker();
    test_same_operation_is_idempotent();
    test_replicas_converge_with_different_operation_orders();
    test_merge_matches_applying_other_register_state();

    std::cout << "All LWW register tests passed.\n";
    return 0;
}
