#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>
#include <optional>

#include "protocol/message.hpp"

using namespace crdt;

TEST_CASE("RGA ID JSON round-trips", "[protocol]") {
    const RGAID id{7, 42};

    const auto parsed = protocol::parse_id(protocol::serialize_id(id));

    REQUIRE(parsed.has_value());
    REQUIRE(*parsed == id);
}

TEST_CASE("RGA node JSON round-trips", "[protocol]") {
    const RGANode node{'x', RGAID{1, 2}, RGAID{3, 4}, true};

    const auto parsed = protocol::parse_node(protocol::serialize_node(node));

    REQUIRE(parsed.has_value());
    REQUIRE(parsed->value == node.value);
    REQUIRE(parsed->id == node.id);
    REQUIRE(parsed->parent_id == node.parent_id);
    REQUIRE(parsed->tombstoned == node.tombstoned);
}

TEST_CASE("Insert and delete operations round-trip", "[protocol]") {
    const InsertOp insert{'a', RGAID{1, 2}, RGAID{0, 0}};
    const DeleteOp delete_op{RGAID{1, 2}};

    const auto parsed_insert = protocol::parse_insert(protocol::serialize_insert(insert));
    const auto parsed_delete = protocol::parse_delete(protocol::serialize_delete(delete_op));

    REQUIRE(parsed_insert.has_value());
    REQUIRE(parsed_insert->value == insert.value);
    REQUIRE(parsed_insert->id == insert.id);
    REQUIRE(parsed_insert->parent_id == insert.parent_id);

    REQUIRE(parsed_delete.has_value());
    REQUIRE(parsed_delete->target_id == delete_op.target_id);
}

TEST_CASE("Unicode insert operation round-trips", "[protocol]") {
    const InsertOp insert{"å", RGAID{1, 2}, RGAID{0, 0}};

    const auto parsed_insert = protocol::parse_insert(protocol::serialize_insert(insert));

    REQUIRE(parsed_insert.has_value());
    REQUIRE(parsed_insert->value == "å");
    REQUIRE(parsed_insert->id == insert.id);
    REQUIRE(parsed_insert->parent_id == insert.parent_id);
}

TEST_CASE("Cursor presence messages round-trip", "[protocol]") {
    const protocol::CursorUpdate cursor{RGAID{1, 2}, 4};

    const auto parsed_cursor = protocol::parse_cursor_update(protocol::serialize_cursor_update(cursor));
    REQUIRE(parsed_cursor.has_value());
    REQUIRE(parsed_cursor->anchor_id.has_value());
    REQUIRE(*parsed_cursor->anchor_id == *cursor.anchor_id);
    REQUIRE(parsed_cursor->cursor_index == cursor.cursor_index);

    const auto remote_cursor = protocol::serialize_remote_cursor(9, cursor);
    REQUIRE(remote_cursor.at("type") == "remote_cursor");
    REQUIRE(remote_cursor.at("peer_id") == 9);
    REQUIRE(remote_cursor.at("cursor_index") == 4);

    const protocol::CursorUpdate document_start{std::nullopt, 0};
    const auto parsed_start = protocol::parse_cursor_update(protocol::serialize_cursor_update(document_start));
    REQUIRE(parsed_start.has_value());
    REQUIRE_FALSE(parsed_start->anchor_id.has_value());
    REQUIRE(parsed_start->cursor_index == 0);

    const auto browser_message = nlohmann::json::parse(
        R"({"type":"cursor_update","anchor_id":null,"cursor_index":0})"
    );
    const auto parsed_browser_cursor = protocol::parse_cursor_update(browser_message);
    REQUIRE(parsed_browser_cursor.has_value());
    REQUIRE(parsed_browser_cursor->cursor_index == 0);

    const auto browser_anchor_message = nlohmann::json::parse(
        R"({"type":"cursor_update","anchor_id":{"peer_id":1,"sequence":2},"cursor_index":4})"
    );
    const auto parsed_browser_anchor = protocol::parse_cursor_update(browser_anchor_message);
    REQUIRE(parsed_browser_anchor.has_value());
    REQUIRE(parsed_browser_anchor->anchor_id.has_value());
    REQUIRE(*parsed_browser_anchor->anchor_id == RGAID{1, 2});
    REQUIRE(parsed_browser_anchor->cursor_index == 4);
}

TEST_CASE("Peer left message includes peer id", "[protocol]") {
    const auto message = protocol::serialize_peer_left(7);

    REQUIRE(message.at("type") == "peer_left");
    REQUIRE(message.at("peer_id") == 7);
}

TEST_CASE("Counter messages validate and serialize", "[protocol]") {
    const auto update = nlohmann::json{
        {"type", "counter_update"},
        {"counter", "cookies"},
        {"replica_id", "cookie-client-a"},
        {"count", 12},
    };
    const auto parsed = protocol::parse_counter_update(update);

    REQUIRE(parsed.has_value());
    REQUIRE(parsed->counter == "cookies");
    REQUIRE(parsed->replica_id == "cookie-client-a");
    REQUIRE(parsed->count == 12);
    REQUIRE(protocol::validate_client_message(update).ok);

    const auto request = nlohmann::json{
        {"type", "counter_state_request"},
        {"counter", "cookies"},
        {"replica_id", "cookie-client-a"},
    };
    const auto parsed_request = protocol::parse_counter_state_request(request);

    REQUIRE(parsed_request.has_value());
    REQUIRE(parsed_request->counter == "cookies");
    REQUIRE(parsed_request->replica_id == "cookie-client-a");
    REQUIRE(protocol::message_type(request) == protocol::MessageType::CounterStateRequest);
    REQUIRE(protocol::validate_client_message(request).ok);

    const auto state = protocol::serialize_counter_state("cookies", 42, 12, "cookie-client-a");
    REQUIRE(state.at("type") == "counter_state");
    REQUIRE(state.at("counter") == "cookies");
    REQUIRE(state.at("replica_id") == "cookie-client-a");
    REQUIRE(state.at("value") == 42);
    REQUIRE(state.at("own_count") == 12);
}

TEST_CASE("Save request is a valid client message", "[protocol]") {
    const auto message = nlohmann::json{{"type", "save_request"}};

    REQUIRE(protocol::message_type(message) == protocol::MessageType::SaveRequest);
    REQUIRE(protocol::message_type_name(protocol::MessageType::SaveRequest) == "save_request");
    REQUIRE(protocol::validate_client_message(message).ok);
}

TEST_CASE("Malformed client messages are rejected", "[protocol]") {
    const auto missing_parent = nlohmann::json{
        {"type", "insert"},
        {"value", "a"},
        {"id", protocol::serialize_id(RGAID{1, 1})},
    };

    const auto unknown = nlohmann::json{{"type", "something_else"}};

    REQUIRE_FALSE(protocol::validate_client_message(missing_parent).ok);
    REQUIRE_FALSE(protocol::validate_client_message(unknown).ok);

    REQUIRE_FALSE(protocol::validate_client_message(nlohmann::json{
        {"type", "cursor_update"},
        {"anchor_id", protocol::serialize_id(RGAID{1, 1})},
    }).ok);

    REQUIRE_FALSE(protocol::validate_client_message(nlohmann::json{
        {"type", "cursor_update"},
        {"anchor_id", nullptr},
        {"cursor_index", -1},
    }).ok);

    REQUIRE_FALSE(protocol::validate_client_message(nlohmann::json{
        {"type", "counter_update"},
        {"counter", "cookies"},
        {"count", -1},
        {"replica_id", "cookie-client-a"},
    }).ok);

    REQUIRE_FALSE(protocol::validate_client_message(nlohmann::json{
        {"type", "counter_update"},
        {"counter", "cookies"},
        {"count", 1},
    }).ok);

    REQUIRE_FALSE(protocol::validate_client_message(nlohmann::json{
        {"type", "counter_update"},
        {"counter", "other"},
        {"count", 1},
        {"replica_id", "cookie-client-a"},
    }).ok);

    REQUIRE_FALSE(protocol::validate_client_message(nlohmann::json{
        {"type", "counter_state_request"},
        {"counter", "cookies"},
        {"replica_id", "bad replica id"},
    }).ok);

    REQUIRE(protocol::validate_client_message(nlohmann::json{
        {"type", "cursor_update"},
        {"anchor_id", nullptr},
        {"cursor_index", std::uint32_t{0}},
    }).ok);
}

TEST_CASE("Sync state contains all nodes and tombstones", "[protocol]") {
    RGA document(1);
    const auto a = document.local_insert('a', RGAID::root());
    document.local_insert('b', a.id);
    document.local_delete(a.id);

    const auto snapshot = protocol::serialize_sync_state(document);

    REQUIRE(snapshot.at("type") == "sync_state");
    REQUIRE(snapshot.at("visible_text") == "b");
    REQUIRE(snapshot.at("nodes").size() == 2);
    REQUIRE(snapshot.at("nodes").at(0).at("tombstoned") == true);
}

TEST_CASE("Sync state contains Unicode node values", "[protocol]") {
    RGA document(1);
    document.local_insert("æ", RGAID::root());

    const auto snapshot = protocol::serialize_sync_state(document);
    const auto loaded = protocol::document_from_snapshot(snapshot, 2);

    REQUIRE(snapshot.at("visible_text") == "æ");
    REQUIRE(snapshot.at("nodes").at(0).at("value") == "æ");
    REQUIRE(loaded.to_string() == "æ");
}
