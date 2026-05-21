#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "crdt/RGA.hpp"
#include "crdt/lww_register.hpp"

namespace protocol {

/**
 * @brief Enumeration of every message type exchanged over the WebSocket.
 *
 * Client-to-server types include Insert, Delete, SyncRequest, SaveRequest,
 * CursorUpdate, CounterUpdate and SetTitle. Server-to-client types include
 * AssignPeerId, SyncState, RemoteInsert, RemoteDelete, RemoteCursor,
 * PeerLeft, CounterState and Error. Unknown is returned by message_type
 * when the incoming JSON has no recognised type field.
 */
enum class MessageType {
    AssignPeerId,
    SyncState,
    Insert,
    Delete,
    SyncRequest,
    SaveRequest,
    RemoteInsert,
    RemoteDelete,
    CursorUpdate,
    RemoteCursor,
    PeerLeft,
    CounterUpdate,
    CounterState,
    SetTitle,
    Error,
    Unknown
};

/**
 * @brief Ephemeral cursor position broadcast between clients.
 *
 * Cursors are not part of any CRDT. They are relayed best-effort because
 * a stale position has no meaning after the underlying text has changed.
 */
struct CursorUpdate {
    std::optional<crdt::RGAID> anchor_id;  ///< RGA node the cursor sits after, or empty if at the start.
    std::uint32_t cursor_index = 0;        ///< Fallback grapheme index used when the anchor cannot be located.
};

/**
 * @brief Incoming update for a named counter (currently only "cookies").
 *
 * The count is interpreted as the sender's current absolute total, not as
 * a delta. The server uses G-Counter merge semantics (max per peer) to
 * integrate it.
 */
struct CounterUpdate {
    std::string counter;        ///< Counter name. Only "cookies" is recognised.
    std::uint64_t count = 0;    ///< Sender's current absolute count for that counter.
};

/**
 * @brief Result of validating an incoming client message.
 */
struct ParseResult {
    bool ok = false;     ///< True if the message passed validation.
    std::string error;   ///< Human-readable error string, populated only when ok is false.
};

/**
 * @brief Inspect a JSON message and return its MessageType.
 *
 * Returns MessageType::Unknown if the message is not a JSON object or
 * lacks a recognised "type" field.
 */
MessageType message_type(const nlohmann::json& message);

/**
 * @brief Map a MessageType back to the wire-format string used in JSON.
 */
std::string message_type_name(MessageType type);

/// @brief Serialize an RGAID to its JSON representation.
nlohmann::json serialize_id(const crdt::RGAID& id);

/// @brief Serialize a full RGA node (including tombstone flag) for use in snapshots.
nlohmann::json serialize_node(const crdt::RGANode& node);

/**
 * @brief Serialize an insert operation.
 * @param op    The operation produced by RGA::local_insert or received as a remote op.
 * @param type  Output message type. Use Insert for client-to-server and RemoteInsert for broadcasts.
 */
nlohmann::json serialize_insert(const crdt::InsertOp& op, MessageType type = MessageType::Insert);

/**
 * @brief Serialize a delete operation.
 * @param op    The operation produced by RGA::local_delete or received as a remote op.
 * @param type  Output message type. Use Delete for client-to-server and RemoteDelete for broadcasts.
 */
nlohmann::json serialize_delete(const crdt::DeleteOp& op, MessageType type = MessageType::Delete);

/// @brief Build the assign_peer_id message sent to a newly connected client.
nlohmann::json serialize_assign_peer_id(std::uint32_t peer_id);

/// @brief Build a sync_state message containing the full document (without title).
nlohmann::json serialize_sync_state(const crdt::RGA& document);

/// @brief Build a sync_state message containing the full document and title register.
nlohmann::json serialize_sync_state(
    const crdt::RGA& document,
    const crdt::LWWRegister<std::string>& title
);

/// @brief Build a set_title broadcast message from an LWW operation.
nlohmann::json serialize_set_title(const crdt::LWWOperation<std::string>& op);

/// @brief Build a title_state message from the current LWW register state.
nlohmann::json serialize_title_state(const crdt::LWWRegister<std::string>& title);

/// @brief Serialize a local cursor update for transmission to the server.
nlohmann::json serialize_cursor_update(const CursorUpdate& cursor);

/// @brief Build a remote_cursor message that the server broadcasts to other peers.
nlohmann::json serialize_remote_cursor(std::uint32_t peer_id, const CursorUpdate& cursor);

/// @brief Build a peer_left message broadcast when a peer disconnects.
nlohmann::json serialize_peer_left(std::uint32_t peer_id);

/**
 * @brief Build a counter_state message for one recipient.
 * @param counter     Counter name (currently only "cookies").
 * @param value       Total value across all peers.
 * @param own_count   The recipient's own slot in the G-Counter, used by the
 *                    client to reconcile its local optimistic count with the
 *                    server's confirmed total.
 */
nlohmann::json serialize_counter_state(
    const std::string& counter,
    std::uint64_t value,
    std::uint64_t own_count
);

/// @brief Build an error message that the server can send back to a client.
nlohmann::json serialize_error(const std::string& message);

/// @brief Parse an RGAID. Returns std::nullopt if the JSON is malformed.
std::optional<crdt::RGAID> parse_id(const nlohmann::json& value);

/// @brief Parse a full RGA node (used when loading a snapshot). Returns std::nullopt if malformed.
std::optional<crdt::RGANode> parse_node(const nlohmann::json& value);

/// @brief Parse an insert operation. Returns std::nullopt if any required field is missing.
std::optional<crdt::InsertOp> parse_insert(const nlohmann::json& message);

/// @brief Parse a delete operation. Returns std::nullopt if any required field is missing.
std::optional<crdt::DeleteOp> parse_delete(const nlohmann::json& message);

/// @brief Parse a cursor update. Returns std::nullopt if malformed.
std::optional<CursorUpdate> parse_cursor_update(const nlohmann::json& message);

/// @brief Parse a counter update. Returns std::nullopt if the counter name is unknown or fields are missing.
std::optional<CounterUpdate> parse_counter_update(const nlohmann::json& message);

/// @brief Parse a set_title operation. Returns std::nullopt if malformed.
std::optional<crdt::LWWOperation<std::string>> parse_set_title(const nlohmann::json& message);

/**
 * @brief Validate an incoming client message structurally.
 *
 * Checks that the JSON is an object, has a recognised type, and contains
 * the fields required for that type. Does not apply the operation. The
 * returned ParseResult carries an error string when validation fails.
 */
ParseResult validate_client_message(const nlohmann::json& message);

/**
 * @brief Rebuild a fresh RGA replica from a persisted snapshot.
 * @param snapshot  JSON object previously produced by DocumentState::snapshot_json.
 * @param peer_id   Peer ID to assign to the reconstructed replica.
 */
crdt::RGA document_from_snapshot(const nlohmann::json& snapshot, std::uint32_t peer_id);

}  // namespace protocol
