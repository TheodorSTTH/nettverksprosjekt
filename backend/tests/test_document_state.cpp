#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "protocol/message.hpp"
#include "server/document_state.hpp"

using namespace crdt;

namespace {

std::filesystem::path test_snapshot_path(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

}  // namespace

TEST_CASE("DocumentState applies insert with known parent", "[server]") {
    const auto path = test_snapshot_path("crdt_doc_state_insert.json");
    std::filesystem::remove(path);

    server::DocumentState state(0, path);

    const auto first = state.apply_insert(InsertOp{'a', RGAID{1, 1}, RGAID::root()});
    const auto second = state.apply_insert(InsertOp{'b', RGAID{1, 2}, RGAID{1, 1}});

    REQUIRE(first.status == server::ApplyStatus::Applied);
    REQUIRE(second.status == server::ApplyStatus::Applied);
    REQUIRE(state.visible_text() == "ab");
}

TEST_CASE("DocumentState rejects insert with missing parent", "[server]") {
    const auto path = test_snapshot_path("crdt_doc_state_missing_parent.json");
    std::filesystem::remove(path);

    server::DocumentState state(0, path);

    const auto result = state.apply_insert(InsertOp{'b', RGAID{1, 2}, RGAID{1, 1}});

    REQUIRE(result.status == server::ApplyStatus::MissingParent);
    REQUIRE(state.visible_text().empty());
}

TEST_CASE("DocumentState tombstones existing delete target", "[server]") {
    const auto path = test_snapshot_path("crdt_doc_state_delete.json");
    std::filesystem::remove(path);

    server::DocumentState state(0, path);
    state.apply_insert(InsertOp{'a', RGAID{1, 1}, RGAID::root()});

    const auto result = state.apply_delete(DeleteOp{RGAID{1, 1}});

    REQUIRE(result.status == server::ApplyStatus::Applied);
    REQUIRE(state.visible_text().empty());
    REQUIRE(state.snapshot_json().at("nodes").at(0).at("tombstoned") == true);
}

TEST_CASE("DocumentState rejects delete with missing target", "[server]") {
    const auto path = test_snapshot_path("crdt_doc_state_missing_delete.json");
    std::filesystem::remove(path);

    server::DocumentState state(0, path);

    const auto result = state.apply_delete(DeleteOp{RGAID{9, 9}});

    REQUIRE(result.status == server::ApplyStatus::MissingTarget);
}

TEST_CASE("Snapshot save and load preserves CRDT state", "[server]") {
    const auto path = test_snapshot_path("crdt_doc_state_snapshot.json");
    std::filesystem::remove(path);

    {
        server::DocumentState state(0, path);
        state.apply_insert(InsertOp{'a', RGAID{1, 1}, RGAID::root()});
        state.apply_insert(InsertOp{'b', RGAID{1, 2}, RGAID{1, 1}});
        state.apply_delete(DeleteOp{RGAID{1, 1}});
        state.save();
    }

    server::DocumentState loaded(0, path);
    loaded.load();

    REQUIRE(loaded.visible_text() == "b");
    REQUIRE(loaded.snapshot_json().at("nodes").size() == 2);
    REQUIRE(loaded.snapshot_json().at("nodes").at(0).at("tombstoned") == true);

    std::filesystem::remove(path);
}

TEST_CASE("Snapshot save and load preserves Unicode text", "[server]") {
    const auto path = test_snapshot_path("crdt_doc_state_unicode_snapshot.json");
    std::filesystem::remove(path);

    {
        server::DocumentState state(0, path);
        state.apply_insert(InsertOp{"æ", RGAID{1, 1}, RGAID::root()});
        state.apply_insert(InsertOp{"ø", RGAID{1, 2}, RGAID{1, 1}});
        state.apply_insert(InsertOp{"å", RGAID{1, 3}, RGAID{1, 2}});
        state.save();
    }

    server::DocumentState loaded(0, path);
    loaded.load();

    REQUIRE(loaded.visible_text() == "æøå");
    REQUIRE(loaded.snapshot_json().at("nodes").at(0).at("value") == "æ");

    std::filesystem::remove(path);
}

TEST_CASE("Cookie counter uses GCounter max semantics per peer", "[server]") {
    const auto path = test_snapshot_path("crdt_doc_state_cookie_counter.json");
    std::filesystem::remove(path);

    server::DocumentState state(0, path);

    REQUIRE(state.apply_cookie_count(1, 3));
    REQUIRE_FALSE(state.apply_cookie_count(1, 2));
    REQUIRE(state.apply_cookie_count(2, 4));

    const auto peer_one_state = state.cookie_counter_state(1);
    REQUIRE(peer_one_state.at("value") == 7);
    REQUIRE(peer_one_state.at("own_count") == 3);

    const auto peer_two_state = state.cookie_counter_state(2);
    REQUIRE(peer_two_state.at("value") == 7);
    REQUIRE(peer_two_state.at("own_count") == 4);
}

TEST_CASE("Cookie counter persists in document snapshot", "[server]") {
    const auto path = test_snapshot_path("crdt_doc_state_cookie_counter_snapshot.json");
    std::filesystem::remove(path);

    {
        server::DocumentState state(0, path);
        state.apply_cookie_count(1, 8);
        state.apply_cookie_count(2, 5);
        state.save();
    }

    server::DocumentState loaded(0, path);
    loaded.load();

    REQUIRE(loaded.cookie_counter_state(1).at("value") == 13);
    REQUIRE(loaded.cookie_counter_state(1).at("own_count") == 8);
    REQUIRE(loaded.snapshot_json().at("cookie_counter").at("counts").at("2") == 5);

    std::filesystem::remove(path);
}
