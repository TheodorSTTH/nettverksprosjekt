#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace crdt {

/**
 * @brief Grow-only counter CRDT, used to track total cookie clicks across replicas.
 *
 * Each replica maintains its own slot in a map keyed by a stable replica
 * identifier. The total value of the counter is the sum of all slots.
 * Merging two replicas takes the maximum per replica, which is what makes
 * the counter convergent.
 * Any number of merges in any order produces the same final state.
 *
 * The CRDT properties hold:
 *  - Commutativity. Merging A into B yields the same state as B into A.
 *  - Idempotency. Merging a replica with itself does not change it.
 *  - Associativity. The order in which replicas are merged does not matter.
 *
 * The counter is grow-only. Slots can only increase. Decrementing or
 * removing a slot would break the G-Counter model and require a different
 * CRDT, such as a PN-Counter.
 */
class GCounter {
    public:
        /**
         * @brief Merge one replica's absolute count into this counter.
         *
         * Implements the per-replica max rule that is at the heart of G-Counter
         * convergence. Sending the same count twice (e.g. on retry) is safe
         * because the second call is a no-op.
         *
         * @param node_id  Stable identifier of the replica that owns the slot.
         * @param count    Absolute count for that replica, not an increment/delta.
         * @return true if the slot was updated, false if the incoming count
         *         was less than or equal to the current value.
         */
        bool merge_count(const std::string& node_id, std::uint64_t count);

        /**
         * @brief Merge another counter into this one by taking the max per replica slot.
         *
         * This is the normal CRDT join operation. It is safe to call repeatedly
         * with the same state, and it is independent of message ordering.
         */
        void merge(const GCounter& other);

        /**
         * @brief Read the current count for a specific replica. Returns 0 if the replica is unknown.
         */
        std::uint64_t count_for(const std::string& node_id) const;

        /**
         * @brief Total counter value across all replica slots.
         */
        std::uint64_t value() const;

        /**
         * @brief Read the underlying per-replica map, used when serializing snapshots.
         *
         * Callers must not mutate the returned map. Snapshot code uses it only
         * to persist and reload the CRDT state.
         */
        const std::unordered_map<std::string, std::uint64_t>& counts() const;

    private:
        std::unordered_map<std::string, std::uint64_t> counts_;
};

}  // namespace crdt
