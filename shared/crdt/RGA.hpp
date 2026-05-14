#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace crdt {

struct RGAID {
    std::uint32_t peer_id = 0;
    std::uint32_t sequence = 0;

    static constexpr RGAID root() noexcept { return RGAID{0, 0}; }
    constexpr bool is_root() const noexcept { return peer_id == 0 && sequence == 0; }

    friend constexpr bool operator==(const RGAID& a, const RGAID& b) noexcept {
        return a.peer_id == b.peer_id && a.sequence == b.sequence;
    }
    friend constexpr bool operator!=(const RGAID& a, const RGAID& b) noexcept {
        return !(a == b);
    }

    // Total order over IDs: higher (sequence, peer_id) is "greater".
    // The placement rule positions greater IDs leftward (closer to the parent).
    friend constexpr bool operator<(const RGAID& a, const RGAID& b) noexcept {
        if (a.sequence != b.sequence) return a.sequence < b.sequence;
        return a.peer_id < b.peer_id;
    }
    friend constexpr bool operator>(const RGAID& a, const RGAID& b) noexcept { return b < a; }
};

struct RGAIDHash {
    std::size_t operator()(const RGAID& id) const noexcept {
        const auto combined = (static_cast<std::uint64_t>(id.peer_id) << 32)
                            ^ static_cast<std::uint64_t>(id.sequence);
        return std::hash<std::uint64_t>{}(combined);
    }
};

struct RGANode {
    char value = '\0';
    RGAID id{};
    RGAID parent_id{};
    bool tombstoned = false;
};

struct InsertOp {
    char value;
    RGAID id;
    RGAID parent_id;
};

struct DeleteOp {
    RGAID target_id;
};

// Replicated Growable Array — sequence CRDT for collaborative text.
// Operation-based: local mutations return an op that callers broadcast;
// remote ops arrive via apply_remote. Convergence is guaranteed by the
// deterministic tie-breaking rule on RGAID and by retaining tombstones
// so late-arriving ops can always find their parent.
class RGA {
public:
    explicit RGA(std::uint32_t peer_id);

    // Local edits. The returned op must be broadcast to peers verbatim.
    InsertOp local_insert(char value, RGAID parent_id);
    DeleteOp local_delete(RGAID target_id);

    // Remote edits. Idempotent: applying the same op twice is a no-op.
    void apply_remote(const InsertOp& op);
    void apply_remote(const DeleteOp& op);

    // Visible text — tombstones skipped.
    std::string to_string() const;

    // Full state, for sync_state messages.
    const std::vector<RGANode>& nodes() const noexcept { return nodes_; }

    std::uint32_t peer_id() const noexcept { return peer_id_; }

private:
    RGAID next_id();
    void observe(std::uint32_t remote_sequence) noexcept;
    void integrate(const RGANode& node);
    std::size_t find_insert_position(RGAID parent_id, RGAID new_id) const;

    std::uint32_t peer_id_;
    std::uint32_t sequence_ = 0;  // Lamport-style counter; advances on local and remote ops.
    std::vector<RGANode> nodes_;  // Linearized depth-first order; tombstones retained.
    std::unordered_map<RGAID, std::size_t, RGAIDHash> index_of_;
};

}  // namespace crdt
