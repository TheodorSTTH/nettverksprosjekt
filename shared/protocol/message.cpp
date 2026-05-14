#include "protocol/message.hpp"

#include <stdexcept>

namespace protocol {
namespace {

std::optional<std::uint32_t> parse_u32_field(const nlohmann::json& value, const char* key) {
    if (!value.contains(key) || !value.at(key).is_number_unsigned()) {
        return std::nullopt;
    }

    const auto number = value.at(key).get<std::uint64_t>();
    if (number > UINT32_MAX) {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(number);
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
        case MessageType::RemoteInsert:
            return "remote_insert";
        case MessageType::RemoteDelete:
            return "remote_delete";
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
    if (type == "remote_insert") return MessageType::RemoteInsert;
    if (type == "remote_delete") return MessageType::RemoteDelete;
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
        {"value", std::string(1, node.value)},
        {"id", serialize_id(node.id)},
        {"parent_id", serialize_id(node.parent_id)},
        {"tombstoned", node.tombstoned},
    };
}

nlohmann::json serialize_insert(const crdt::InsertOp& op, MessageType type) {
    return nlohmann::json{
        {"type", type_string(type)},
        {"value", std::string(1, op.value)},
        {"char", std::string(1, op.value)},
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
    if (text.size() != 1) {
        return std::nullopt;
    }

    const auto id = parse_id(value.at("id"));
    const auto parent_id = parse_id(value.at("parent_id"));
    if (!id || !parent_id) {
        return std::nullopt;
    }

    return crdt::RGANode{text[0], *id, *parent_id, value.at("tombstoned").get<bool>()};
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
    if (text.size() != 1) {
        return std::nullopt;
    }

    const auto id = parse_id(message.at("id"));
    const auto parent_id = parse_id(message.at("parent_id"));
    if (!id || !parent_id) {
        return std::nullopt;
    }

    return crdt::InsertOp{text[0], *id, *parent_id};
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

ParseResult validate_client_message(const nlohmann::json& message) {
    switch (message_type(message)) {
        case MessageType::Insert:
            return parse_insert(message) ? ParseResult{true, {}} : ParseResult{false, "invalid insert message"};
        case MessageType::Delete:
            return parse_delete(message) ? ParseResult{true, {}} : ParseResult{false, "invalid delete message"};
        case MessageType::SyncRequest:
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
