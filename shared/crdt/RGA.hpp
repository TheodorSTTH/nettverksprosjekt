#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace crdt {

/**
 * @brief Unique identifier for a node in the RGA.
 *
 * Each character in the document is tagged with a (peer_id, sequence) pair
 * that is unique across all replicas. The pair is what makes concurrent
 * inserts unambiguous. Instead of saying "insert at index N" (which shifts
 * under concurrent edits), operations say "insert after node with ID X."
 *
 * The reserved ID (0, 0) is the root sentinel, used as the parent for
 * inserts at the very beginning of the document.
 */
struct RGAID {
    std::uint32_t peer_id = 0;   ///< Originating peer. The value 0 is reserved for the root sentinel.
    std::uint32_t sequence = 0;  ///< Per-peer monotonic counter advanced on every local or remote op.

    /// @brief Returns the root sentinel ID used as the implicit parent of the first node.
    static constexpr RGAID root() noexcept { return RGAID{0, 0}; }

    /// @brief True if this ID is the root sentinel.
    constexpr bool is_root() const noexcept { return peer_id == 0 && sequence == 0; }

    friend constexpr bool operator==(const RGAID& a, const RGAID& b) noexcept {
        return a.peer_id == b.peer_id && a.sequence == b.sequence;
    }
    friend constexpr bool operator!=(const RGAID& a, const RGAID& b) noexcept {
        return !(a == b);
    }

    /**
     * @brief Total order on IDs used as the tie-breaking rule for concurrent inserts.
     *
     * Sequence is compared first, then peer_id. A "greater" ID is positioned
     * leftward (closer to the parent) when two nodes share the same parent.
     * The order is total and deterministic, which is what guarantees that
     * every replica resolves concurrent inserts to the same final order.
     */
    friend constexpr bool operator<(const RGAID& a, const RGAID& b) noexcept {
        if (a.sequence != b.sequence) return a.sequence < b.sequence;
        return a.peer_id < b.peer_id;
    }
    friend constexpr bool operator>(const RGAID& a, const RGAID& b) noexcept { return b < a; }
};

/**
 * @brief Hash functor for RGAID, used by std::unordered_map.
 */
struct RGAIDHash {
    std::size_t operator()(const RGAID& id) const noexcept {
        const auto combined = (static_cast<std::uint64_t>(id.peer_id) << 32)
                            ^ static_cast<std::uint64_t>(id.sequence);
        return std::hash<std::uint64_t>{}(combined);
    }
};

/**
 * @brief A single character node in the RGA sequence.
 *
 * Deleted characters are not removed. They are marked with tombstoned = true
 * and kept in the list so that late-arriving inserts referencing them as a
 * parent can still find them.
 */
struct RGANode {
    std::string value;       ///< Character payload, stored as string to support multi-byte UTF-8.
    RGAID id{};              ///< Unique identifier for this node.
    RGAID parent_id{};       ///< ID of the node this was inserted after.
    bool tombstoned = false; ///< If true, the node has been deleted but is retained for structural reasons.

    RGANode() = default;
    RGANode(std::string node_value, RGAID node_id, RGAID node_parent_id, bool node_tombstoned)
        : value(std::move(node_value)),
          id(node_id),
          parent_id(node_parent_id),
          tombstoned(node_tombstoned) {}
    RGANode(char node_value, RGAID node_id, RGAID node_parent_id, bool node_tombstoned)
        : RGANode(std::string(1, node_value), node_id, node_parent_id, node_tombstoned) {}
};

/**
 * @brief Operation describing an insert, returned by local_insert and broadcast to peers.
 */
struct InsertOp {
    std::string value;  ///< Character to insert.
    RGAID id;           ///< Unique ID assigned to the new node.
    RGAID parent_id;    ///< ID of the node to insert after.

    InsertOp() = default;
    InsertOp(std::string op_value, RGAID op_id, RGAID op_parent_id)
        : value(std::move(op_value)),
          id(op_id),
          parent_id(op_parent_id) {}
    InsertOp(char op_value, RGAID op_id, RGAID op_parent_id)
        : InsertOp(std::string(1, op_value), op_id, op_parent_id) {}
};

/**
 * @brief Operation describing a delete, returned by local_delete and broadcast to peers.
 */
struct DeleteOp {
    RGAID target_id;  ///< ID of the node to tombstone.
};

/**
 * @brief Replicated Growable Array, the sequence CRDT used for the document text.
 *
 * RGA is an operation-based CRDT. Local edits return operations that the
 * caller broadcasts to peers, and remote operations arrive via apply_remote.
 * Convergence is guaranteed by two design choices:
 *
 *  - Every node has a unique (peer_id, sequence) ID, so operations can refer
 *    to positions unambiguously even under concurrent edits.
 *  - Deleted nodes are kept as tombstones, so a remote insert referencing a
 *    previously deleted parent can always find it.
 *
 * The CRDT properties hold:
 *  - Commutativity: applying ops A then B gives the same state as B then A.
 *  - Idempotency: applying the same op twice has no additional effect.
 *  - Associativity: grouping of operations does not affect the final state.
 */
class RGA {
public:
    /**
     * @brief Construct an empty replica.
     * @param peer_id Unique identifier for this replica, used in every locally generated RGAID.
     */
    explicit RGA(std::uint32_t peer_id);

    /**
     * @brief Insert a character (or grapheme cluster) locally.
     * @param value      The character to insert.
     * @param parent_id  ID of the node to insert after. Use RGAID::root() to insert at the start.
     * @return Operation describing the insert. Must be broadcast to peers verbatim.
     */
    InsertOp local_insert(std::string value, RGAID parent_id);
    InsertOp local_insert(char value, RGAID parent_id);

    /**
     * @brief Delete a character locally by marking it as tombstoned.
     * @param target_id  ID of the node to delete.
     * @return Operation describing the delete. Must be broadcast to peers verbatim.
     */
    DeleteOp local_delete(RGAID target_id);

    /**
     * @brief Apply an insert operation received from a remote peer.
     *
     * Idempotent. Applying the same operation twice has no additional effect.
     * If the parent referenced by the operation is unknown, the operation is
     * dropped (the parent must be present, possibly as a tombstone).
     */
    void apply_remote(const InsertOp& op);

    /**
     * @brief Apply a delete operation received from a remote peer.
     *
     * Idempotent. Deleting an already-tombstoned node has no additional effect.
     * If the target ID is unknown, the operation is silently ignored.
     */
    void apply_remote(const DeleteOp& op);

    /**
     * @brief Produce the visible text by walking the node list and skipping tombstones.
     */
    std::string to_string() const;

    /**
     * @brief Read the full node list, including tombstones, for use in sync_state messages.
     */
    const std::vector<RGANode>& nodes() const noexcept { return nodes_; }

    /**
     * @brief This replica's peer ID.
     */
    std::uint32_t peer_id() const noexcept { return peer_id_; }

private:
    RGAID next_id();
    void observe(std::uint32_t remote_sequence) noexcept;
    void integrate(const RGANode& node);
    std::size_t find_insert_position(RGAID parent_id, RGAID new_id) const;

    std::uint32_t peer_id_;
    std::uint32_t sequence_ = 0;  ///< Lamport-style counter that advances on local and remote ops.
    std::vector<RGANode> nodes_;  ///< Linearized depth-first order. Tombstones are retained.
    std::unordered_map<RGAID, std::size_t, RGAIDHash> index_of_;
};

}  // namespace crdt
