#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "crdt/clock.hpp"

namespace crdt {

struct LWWMetadata {
    std::uint64_t timestamp = 0;
    std::uint32_t peer_id = 0;
};

inline bool operator==(const LWWMetadata& left, const LWWMetadata& right) {
    return left.timestamp == right.timestamp && left.peer_id == right.peer_id;
}

inline bool operator!=(const LWWMetadata& left, const LWWMetadata& right) {
    return !(left == right);
}

// Total ordering on writes: timestamp first, peer_id as deterministic tiebreaker.
bool is_newer(const LWWMetadata& candidate, const LWWMetadata& current);

template <typename T>
struct LWWOperation {
    T value;
    LWWMetadata metadata;
};

template <typename T>
class LWWRegister {
public:
    explicit LWWRegister(std::uint32_t peer_id, T initial_value = T{});

    // Local write. Ticks the Lamport clock, replaces the value, and returns
    // the operation that callers can broadcast to other replicas.
    LWWOperation<T> set(T new_value);

    // Apply a remote operation. Observes the remote timestamp into the local
    // clock, then keeps whichever write wins under is_newer().
    void apply(const LWWOperation<T>& operation);

    // Convenience: merge state from another replica directly.
    void merge(const LWWRegister<T>& other);

    const T& value() const;
    LWWMetadata metadata() const;

private:
    T value_;
    LWWMetadata metadata_;
    LamportClock clock_;
};

// Explicit instantiations live in lww_register.cpp.
extern template class LWWRegister<std::string>;

}  // namespace crdt
