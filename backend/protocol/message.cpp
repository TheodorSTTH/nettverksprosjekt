#include "protocol/message.hpp"

#include <cctype>
#include <stdexcept>

namespace protocol {
namespace {

std::optional<std::uint32_t> parse_u32_field(const nlohmann::json& value, const char* key) {
    if (!value.contains(key) ||
        (!value.at(key).is_number_integer() && !value.at(key).is_number_unsigned())) {
        return std::nullopt;
    }

    std::uint64_t number = 0;
    if (value.at(key).is_number_unsigned()) {
        number = value.at(key).get<std::uint64_t>();
    } else {
        const auto signed_number = value.at(key).get<std::int64_t>();
        if (signed_number < 0) {
            return std::nullopt;
        }
        number = static_cast<std::uint64_t>(signed_number);
    }

    if (number > UINT32_MAX) {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(number);
}

std::optional<std::uint64_t> parse_u64_field(const nlohmann::json& value, const char* key) {
    if (!value.contains(key) ||
        (!value.at(key).is_number_integer() && !value.at(key).is_number_unsigned())) {
        return std::nullopt;
    }

    if (value.at(key).is_number_unsigned()) {
        return value.at(key).get<std::uint64_t>();
    }

    const auto signed_number = value.at(key).get<std::int64_t>();
    if (signed_number < 0) {
        return std::nullopt;
    }

    return static_cast<std::uint64_t>(signed_number);
}

bool is_valid_replica_id(const std::string& replica_id) {
    if (replica_id.empty() || replica_id.size() > 128) {
        return false;
    }

    for (const auto character : replica_id) {
        const auto byte = static_cast<unsigned char>(character);
        if (!std::isalnum(byte) && character != '-' && character != '_') {
            return false;
        }
    }

    return true;
}

std::string type_string(MessageType type) {
    switch (type) {
        case MessageType::AssignPeerId:
            return "assign_peer_id";
        case MessageType::SyncState:
            return "sync_state";
        case MessageType::Insert:
            return "insert";
        case MessageType::Delete:
            return "delete";
        case MessageType::SyncRequest:
            return "sync_request";
        case MessageType::SaveRequest:
            return "save_request";
        case MessageType::RemoteInsert:
            return "remote_insert";
        case MessageType::RemoteDelete:
            return "remote_delete";
        case MessageType::CursorUpdate:
            return "cursor_update";
        case MessageType::RemoteCursor:
            return "remote_cursor";
        case MessageType::PeerLeft:
            return "peer_left";
        case MessageType::CounterUpdate:
            return "counter_update";
        case MessageType::CounterStateRequest:
            return "counter_state_request";
        case MessageType::CounterState:
            return "counter_state";
        case MessageType::SetTitle:
            return "set_title";
        case MessageType::Error:
            return "error";
        case MessageType::Unknown:
            return "unknown";
    }

    return "unknown";
}

}  // namespace

MessageType message_type(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("type") || !message.at("type").is_string()) {
        return MessageType::Unknown;
    }

    const auto type = message.at("type").get<std::string>();
    if (type == "assign_peer_id") return MessageType::AssignPeerId;
    if (type == "sync_state") return MessageType::SyncState;
    if (type == "insert") return MessageType::Insert;
    if (type == "delete") return MessageType::Delete;
    if (type == "sync_request") return MessageType::SyncRequest;
    if (type == "save_request") return MessageType::SaveRequest;
    if (type == "remote_insert") return MessageType::RemoteInsert;
    if (type == "remote_delete") return MessageType::RemoteDelete;
    if (type == "cursor_update") return MessageType::CursorUpdate;
    if (type == "remote_cursor") return MessageType::RemoteCursor;
    if (type == "peer_left") return MessageType::PeerLeft;
    if (type == "counter_update") return MessageType::CounterUpdate;
    if (type == "counter_state_request") return MessageType::CounterStateRequest;
    if (type == "counter_state") return MessageType::CounterState;
    if (type == "set_title") return MessageType::SetTitle;
    if (type == "error") return MessageType::Error;
    return MessageType::Unknown;
}

std::string message_type_name(MessageType type) {
    return type_string(type);
}

nlohmann::json serialize_id(const crdt::RGAID& id) {
    return nlohmann::json{{"peer_id", id.peer_id}, {"sequence", id.sequence}};
}

nlohmann::json serialize_node(const crdt::RGANode& node) {
    return nlohmann::json{
        {"value", node.value},
        {"id", serialize_id(node.id)},
        {"parent_id", serialize_id(node.parent_id)},
        {"tombstoned", node.tombstoned},
    };
}

nlohmann::json serialize_insert(const crdt::InsertOp& op, MessageType type) {
    return nlohmann::json{
        {"type", type_string(type)},
        {"value", op.value},
        {"char", op.value},
        {"id", serialize_id(op.id)},
        {"parent_id", serialize_id(op.parent_id)},
    };
}

nlohmann::json serialize_delete(const crdt::DeleteOp& op, MessageType type) {
    return nlohmann::json{
        {"type", type_string(type)},
        {"target_id", serialize_id(op.target_id)},
    };
}

nlohmann::json serialize_assign_peer_id(std::uint32_t peer_id) {
    return nlohmann::json{{"type", "assign_peer_id"}, {"peer_id", peer_id}};
}

nlohmann::json serialize_sync_state(const crdt::RGA& document) {
    nlohmann::json nodes = nlohmann::json::array();
    for (const auto& node : document.nodes()) {
        nodes.push_back(serialize_node(node));
    }

    return nlohmann::json{
        {"type", "sync_state"},
        {"nodes", nodes},
        {"visible_text", document.to_string()},
    };
}

nlohmann::json serialize_sync_state(
    const crdt::RGA& document,
    const crdt::LWWRegister<std::string>& title
) {
    auto state = serialize_sync_state(document);
    state["title"] = serialize_title_state(title);
    return state;
}

nlohmann::json serialize_title_state(const crdt::LWWRegister<std::string>& title) {
    const auto meta = title.metadata();
    return nlohmann::json{
        {"value", title.value()},
        {"timestamp", meta.timestamp},
        {"peer_id", meta.peer_id},
    };
}

nlohmann::json serialize_set_title(const crdt::LWWOperation<std::string>& op) {
    return nlohmann::json{
        {"type", "set_title"},
        {"value", op.value},
        {"timestamp", op.metadata.timestamp},
        {"peer_id", op.metadata.peer_id},
    };
}

nlohmann::json serialize_cursor_update(const CursorUpdate& cursor) {
    return nlohmann::json{
        {"type", "cursor_update"},
        {"anchor_id", cursor.anchor_id ? serialize_id(*cursor.anchor_id) : nlohmann::json(nullptr)},
        {"cursor_index", cursor.cursor_index},
    };
}

nlohmann::json serialize_remote_cursor(std::uint32_t peer_id, const CursorUpdate& cursor) {
    auto message = serialize_cursor_update(cursor);
    message["type"] = "remote_cursor";
    message["peer_id"] = peer_id;
    return message;
}

nlohmann::json serialize_peer_left(std::uint32_t peer_id) {
    return nlohmann::json{{"type", "peer_left"}, {"peer_id", peer_id}};
}

nlohmann::json serialize_counter_state(
    const std::string& counter,
    std::uint64_t value,
    std::uint64_t own_count,
    const std::string& replica_id
) {
    auto message = nlohmann::json{
        {"type", "counter_state"},
        {"counter", counter},
        {"value", value},
        {"own_count", own_count},
    };

    if (!replica_id.empty()) {
        message["replica_id"] = replica_id;
    }

    return message;
}

nlohmann::json serialize_error(const std::string& message) {
    return nlohmann::json{{"type", "error"}, {"message", message}};
}

std::optional<crdt::RGAID> parse_id(const nlohmann::json& value) {
    if (!value.is_object()) {
        return std::nullopt;
    }

    const auto peer_id = parse_u32_field(value, "peer_id");
    const auto sequence = parse_u32_field(value, "sequence");
    if (!peer_id || !sequence) {
        return std::nullopt;
    }

    return crdt::RGAID{*peer_id, *sequence};
}

std::optional<crdt::RGANode> parse_node(const nlohmann::json& value) {
    if (!value.is_object() || !value.contains("value") || !value.at("value").is_string() ||
        !value.contains("id") || !value.contains("parent_id") || !value.contains("tombstoned") ||
        !value.at("tombstoned").is_boolean()) {
        return std::nullopt;
    }

    const auto text = value.at("value").get<std::string>();
    if (text.empty()) {
        return std::nullopt;
    }

    const auto id = parse_id(value.at("id"));
    const auto parent_id = parse_id(value.at("parent_id"));
    if (!id || !parent_id) {
        return std::nullopt;
    }

    return crdt::RGANode{text, *id, *parent_id, value.at("tombstoned").get<bool>()};
}

std::optional<crdt::InsertOp> parse_insert(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("id") || !message.contains("parent_id")) {
        return std::nullopt;
    }

    const char* value_key = nullptr;
    if (message.contains("value") && message.at("value").is_string()) {
        value_key = "value";
    } else if (message.contains("char") && message.at("char").is_string()) {
        value_key = "char";
    }
    if (value_key == nullptr) {
        return std::nullopt;
    }

    const auto text = message.at(value_key).get<std::string>();
    if (text.empty()) {
        return std::nullopt;
    }

    const auto id = parse_id(message.at("id"));
    const auto parent_id = parse_id(message.at("parent_id"));
    if (!id || !parent_id) {
        return std::nullopt;
    }

    return crdt::InsertOp{text, *id, *parent_id};
}

std::optional<crdt::DeleteOp> parse_delete(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("target_id")) {
        return std::nullopt;
    }

    const auto target_id = parse_id(message.at("target_id"));
    if (!target_id) {
        return std::nullopt;
    }

    return crdt::DeleteOp{*target_id};
}

std::optional<CursorUpdate> parse_cursor_update(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("anchor_id") || !message.contains("cursor_index") ||
        (!message.at("cursor_index").is_number_integer() && !message.at("cursor_index").is_number_unsigned())) {
        return std::nullopt;
    }

    std::uint64_t cursor_index = 0;
    if (message.at("cursor_index").is_number_unsigned()) {
        cursor_index = message.at("cursor_index").get<std::uint64_t>();
    } else {
        const auto signed_cursor_index = message.at("cursor_index").get<std::int64_t>();
        if (signed_cursor_index < 0) {
            return std::nullopt;
        }
        cursor_index = static_cast<std::uint64_t>(signed_cursor_index);
    }

    if (cursor_index > UINT32_MAX) {
        return std::nullopt;
    }

    std::optional<crdt::RGAID> anchor_id;
    if (!message.at("anchor_id").is_null()) {
        anchor_id = parse_id(message.at("anchor_id"));
        if (!anchor_id) {
            return std::nullopt;
        }
    }

    return CursorUpdate{anchor_id, static_cast<std::uint32_t>(cursor_index)};
}

std::optional<crdt::LWWOperation<std::string>> parse_set_title(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("value") || !message.at("value").is_string()) {
        return std::nullopt;
    }

    const auto timestamp = parse_u64_field(message, "timestamp");
    const auto peer_id = parse_u32_field(message, "peer_id");
    if (!timestamp || !peer_id) {
        return std::nullopt;
    }

    return crdt::LWWOperation<std::string>{
        message.at("value").get<std::string>(),
        crdt::LWWMetadata{*timestamp, *peer_id},
    };
}

std::optional<CounterUpdate> parse_counter_update(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("counter") || !message.at("counter").is_string() ||
        !message.contains("replica_id") || !message.at("replica_id").is_string()) {
        return std::nullopt;
    }

    const auto counter = message.at("counter").get<std::string>();
    if (counter != "cookies") {
        return std::nullopt;
    }

    const auto replica_id = message.at("replica_id").get<std::string>();
    if (!is_valid_replica_id(replica_id)) {
        return std::nullopt;
    }

    const auto count = parse_u64_field(message, "count");
    if (!count) {
        return std::nullopt;
    }

    return CounterUpdate{counter, replica_id, *count};
}

std::optional<CounterStateRequest> parse_counter_state_request(const nlohmann::json& message) {
    if (!message.is_object() || !message.contains("counter") || !message.at("counter").is_string() ||
        !message.contains("replica_id") || !message.at("replica_id").is_string()) {
        return std::nullopt;
    }

    const auto counter = message.at("counter").get<std::string>();
    if (counter != "cookies") {
        return std::nullopt;
    }

    const auto replica_id = message.at("replica_id").get<std::string>();
    if (!is_valid_replica_id(replica_id)) {
        return std::nullopt;
    }

    return CounterStateRequest{counter, replica_id};
}

ParseResult validate_client_message(const nlohmann::json& message) {
    switch (message_type(message)) {
        case MessageType::Insert:
            return parse_insert(message) ? ParseResult{true, {}} : ParseResult{false, "invalid insert message"};
        case MessageType::Delete:
            return parse_delete(message) ? ParseResult{true, {}} : ParseResult{false, "invalid delete message"};
        case MessageType::CursorUpdate:
            return parse_cursor_update(message) ? ParseResult{true, {}} : ParseResult{false, "invalid cursor update"};
        case MessageType::CounterUpdate:
            return parse_counter_update(message) ? ParseResult{true, {}} : ParseResult{false, "invalid counter update"};
        case MessageType::CounterStateRequest:
            return parse_counter_state_request(message) ? ParseResult{true, {}} : ParseResult{false, "invalid counter state request"};
        case MessageType::SetTitle:
            return parse_set_title(message) ? ParseResult{true, {}} : ParseResult{false, "invalid set_title message"};
        case MessageType::SyncRequest:
        case MessageType::SaveRequest:
            return ParseResult{true, {}};
        default:
            return ParseResult{false, "unknown message type"};
    }
}

crdt::RGA document_from_snapshot(const nlohmann::json& snapshot, std::uint32_t peer_id) {
    if (!snapshot.is_object() || !snapshot.contains("nodes") || !snapshot.at("nodes").is_array()) {
        throw std::invalid_argument("snapshot must contain a nodes array");
    }

    crdt::RGA document(peer_id);
    for (const auto& value : snapshot.at("nodes")) {
        const auto node = parse_node(value);
        if (!node) {
            throw std::invalid_argument("snapshot contains an invalid node");
        }

        document.apply_remote(crdt::InsertOp{node->value, node->id, node->parent_id});
        if (node->tombstoned) {
            document.apply_remote(crdt::DeleteOp{node->id});
        }
    }

    return document;
}

}  // namespace protocol
