#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace crdt {

/**
 * @brief Grow-only counter CRDT, used to track total cookie clicks across all peers.
 *
 * Each peer maintains its own slot in a map keyed by peer identifier. The
 * total value of the counter is the sum of all slots. Merging two replicas
 * takes the maximum per peer, which is what makes the counter convergent.
 * Any number of merges in any order produces the same final state.
 *
 * The CRDT properties hold:
 *  - Commutativity. Merging A into B yields the same state as B into A.
 *  - Idempotency. Merging a replica with itself does not change it.
 *  - Associativity. The order in which replicas are merged does not matter.
 *
 * The counter is grow-only. Slots can only increase. Decrementing or
 * removing a slot would break convergence.
 */
class GCounter {
    public:
        /**
         * @brief Set the slot for a peer to the given count, if larger than the current value.
         *
         * Implements the per-peer max rule that is at the heart of G-Counter
         * convergence. Sending the same count twice (e.g. on retry) is safe
         * because the second call is a no-op.
         *
         * @param node_id  Identifier of the peer that owns the slot (peer_id as string).
         * @param count    The new count for that slot.
         * @return true if the slot was updated, false if the incoming count
         *         was less than or equal to the current value.
         */
        bool merge_count(const std::string& node_id, std::uint64_t count);

        /**
         * @brief Merge another counter into this one by taking the max per peer.
         */
        void merge(const GCounter& other);

        /**
         * @brief Read the current count for a specific peer. Returns 0 if the peer is unknown.
         */
        std::uint64_t count_for(const std::string& node_id) const;

        /**
         * @brief Total counter value across all peers.
         */
        std::uint64_t value() const;

        /**
         * @brief Read the underlying per-peer map, used when serializing snapshots.
         */
        const std::unordered_map<std::string, std::uint64_t>& counts() const;

    private:
        std::unordered_map<std::string, std::uint64_t> counts_;
};

}  // namespace crdt
