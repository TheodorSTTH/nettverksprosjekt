#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "crdt/GCounter.hpp"
#include "crdt/RGA.hpp"
#include "crdt/lww_register.hpp"

namespace server {

/**
 * @brief Outcome of applying a CRDT operation on the server.
 *
 * Applied means the operation was integrated into the document.
 * Duplicate means the operation has been seen before and was a no-op.
 * MissingParent and MissingTarget indicate that the operation could not
 * be applied because a referenced node is unknown to this replica, which
 * normally triggers a sync_request from the client.
 */
enum class ApplyStatus {
    Applied,
    MissingParent,
    MissingTarget,
    Duplicate,
};

/**
 * @brief Result of applying an insert operation, paired with the operation itself.
 */
struct InsertResult {
    ApplyStatus status = ApplyStatus::Applied;
    crdt::InsertOp operation{};
};

/**
 * @brief Result of applying a delete operation, paired with the operation itself.
 */
struct DeleteResult {
    ApplyStatus status = ApplyStatus::Applied;
    crdt::DeleteOp operation{};
};

/**
 * @brief Authoritative server-side document state.
 *
 * Owns the three CRDTs that together describe the document: an RGA for the
 * text content, an LWWRegister for the title, and a G-Counter for the
 * cookie clicker. All accesses are guarded by an internal mutex so the
 * class can be used from the WebSocket event loop and any background
 * worker without external synchronisation.
 *
 * On every applied change the state is persisted to a JSON snapshot file
 * so that the server can resume after a restart with no data loss.
 */
class DocumentState {
public:
    /**
     * @brief Construct a document state and bind it to a snapshot file.
     * @param server_peer_id  Peer ID used for any writes performed by the server itself.
     *                        Currently the server only relays client ops, so this is unused
     *                        for normal operation but reserved for future server-side edits.
     * @param snapshot_path   Filesystem path where snapshots are written and loaded from.
     */
    DocumentState(std::uint32_t server_peer_id, std::filesystem::path snapshot_path);

    /**
     * @brief Load state from the snapshot file, if it exists. Safe to call on a fresh server.
     */
    void load();

    /**
     * @brief Write the current state to the snapshot file as JSON.
     */
    void save() const;

    /**
     * @brief Apply an incoming insert operation to the RGA.
     *
     * Returns the resulting status, including Duplicate when the operation
     * has already been applied (which makes the call a no-op).
     */
    InsertResult apply_insert(const crdt::InsertOp& operation);

    /**
     * @brief Apply an incoming delete operation to the RGA.
     */
    DeleteResult apply_delete(const crdt::DeleteOp& operation);

    /**
     * @brief Merge a peer's cookie count using G-Counter semantics (max per peer).
     * @return true if the peer's slot was updated, false if the incoming count was not larger.
     */
    bool apply_cookie_count(std::uint32_t peer_id, std::uint64_t count);

    /**
     * @brief Apply a remote title write using LWW semantics.
     */
    void apply_set_title(const crdt::LWWOperation<std::string>& operation);

    /**
     * @brief Produce a JSON snapshot suitable for sending to a newly connected client.
     */
    nlohmann::json snapshot_json() const;

    /**
     * @brief Produce the counter_state message for a specific recipient.
     * @param peer_id  Recipient peer ID, used to populate the own_count field.
     */
    nlohmann::json cookie_counter_state(std::uint32_t peer_id) const;

    /**
     * @brief Produce the current title_state message.
     */
    nlohmann::json title_state() const;

    /**
     * @brief Render the document as visible text, skipping tombstones.
     */
    std::string visible_text() const;

private:
    bool has_node(const crdt::RGAID& id) const;
    void load_cookie_counter(const nlohmann::json& snapshot);
    void load_title(const nlohmann::json& snapshot);
    nlohmann::json cookie_counter_json() const;

    mutable std::mutex mutex_;
    crdt::RGA document_;
    crdt::GCounter cookie_counter_;
    crdt::LWWRegister<std::string> title_;
    std::filesystem::path snapshot_path_;
};

}  // namespace server
