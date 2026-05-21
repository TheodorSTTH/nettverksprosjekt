#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "crdt/clock.hpp"

namespace crdt {

/**
 * @brief Metadata attached to every LWW write, used to resolve concurrent updates.
 *
 * The timestamp is a Lamport clock value, and peer_id is the deterministic
 * tiebreaker used when two writes happen at the same logical time.
 */
struct LWWMetadata {
    std::uint64_t timestamp = 0;  ///< Lamport timestamp at the moment of the write.
    std::uint32_t peer_id = 0;    ///< Originating peer, used as a deterministic tiebreaker.
};

inline bool operator==(const LWWMetadata& left, const LWWMetadata& right) {
    return left.timestamp == right.timestamp && left.peer_id == right.peer_id;
}

inline bool operator!=(const LWWMetadata& left, const LWWMetadata& right) {
    return !(left == right);
}

/**
 * @brief Total ordering on writes. Newer timestamp wins, and peer_id breaks ties.
 *
 * Returns true if the candidate metadata is strictly newer than the current
 * metadata under the LWW rule. Used by LWWRegister::apply to decide whether
 * an incoming remote write should overwrite the local value.
 */
bool is_newer(const LWWMetadata& candidate, const LWWMetadata& current);

/**
 * @brief A single LWW write together with the metadata needed to merge it on remote replicas.
 */
template <typename T>
struct LWWOperation {
    T value;
    LWWMetadata metadata;
};

/**
 * @brief Last-Writer-Wins register CRDT.
 *
 * Holds a single value of type T. When two peers write concurrently, the
 * write with the highest Lamport timestamp wins, and ties are broken
 * deterministically by peer_id. This means every replica converges to the
 * same value regardless of message arrival order.
 *
 * In this project the register is used for the document title.
 */
template <typename T>
class LWWRegister {
public:
    /**
     * @brief Construct a register owned by the given peer, optionally with an initial value.
     */
    explicit LWWRegister(std::uint32_t peer_id, T initial_value = T{});

    /**
     * @brief Perform a local write.
     *
     * Ticks the Lamport clock, replaces the value, and returns the operation
     * that callers can broadcast to other replicas.
     */
    LWWOperation<T> set(T new_value);

    /**
     * @brief Apply a remote operation.
     *
     * Observes the remote timestamp into the local clock, then keeps
     * whichever write wins under is_newer.
     */
    void apply(const LWWOperation<T>& operation);

    /**
     * @brief Merge state from another replica directly, equivalent to applying its current operation.
     */
    void merge(const LWWRegister<T>& other);

    /**
     * @brief Read the current value.
     */
    const T& value() const;

    /**
     * @brief Read the metadata associated with the current value.
     */
    LWWMetadata metadata() const;

private:
    T value_;
    LWWMetadata metadata_;
    LamportClock clock_;
};

// Explicit instantiations live in lww_register.cpp.
extern template class LWWRegister<std::string>;

}  // namespace crdt
