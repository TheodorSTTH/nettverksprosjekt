#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "crdt/RGA.hpp"

namespace protocol {

enum class MessageType {
    AssignPeerId,
    SyncState,
    Insert,
    Delete,
    SyncRequest,
    RemoteInsert,
    RemoteDelete,
    Error,
    Unknown
};

struct ParseResult {
    bool ok = false;
    std::string error;
};

MessageType message_type(const nlohmann::json& message);
std::string message_type_name(MessageType type);

nlohmann::json serialize_id(const crdt::RGAID& id);
nlohmann::json serialize_node(const crdt::RGANode& node);
nlohmann::json serialize_insert(const crdt::InsertOp& op, MessageType type = MessageType::Insert);
nlohmann::json serialize_delete(const crdt::DeleteOp& op, MessageType type = MessageType::Delete);
nlohmann::json serialize_assign_peer_id(std::uint32_t peer_id);
nlohmann::json serialize_sync_state(const crdt::RGA& document);
nlohmann::json serialize_error(const std::string& message);

std::optional<crdt::RGAID> parse_id(const nlohmann::json& value);
std::optional<crdt::RGANode> parse_node(const nlohmann::json& value);
std::optional<crdt::InsertOp> parse_insert(const nlohmann::json& message);
std::optional<crdt::DeleteOp> parse_delete(const nlohmann::json& message);

ParseResult validate_client_message(const nlohmann::json& message);
crdt::RGA document_from_snapshot(const nlohmann::json& snapshot, std::uint32_t peer_id);

}  // namespace protocol
