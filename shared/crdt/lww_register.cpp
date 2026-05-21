#include "crdt/lww_register.hpp"

#include <string>
#include <utility>

namespace crdt {

bool is_newer(const LWWMetadata& candidate, const LWWMetadata& current) {
    if (candidate.timestamp != current.timestamp) {
        return candidate.timestamp > current.timestamp;
    }

    // Equal Lamport timestamps mean the writes are concurrent from this
    // register's perspective. peer_id gives every replica the same winner.
    return candidate.peer_id > current.peer_id;
}

template <typename T>
LWWRegister<T>::LWWRegister(std::uint32_t peer_id, T initial_value)
    : value_(std::move(initial_value)), metadata_{0, peer_id}, clock_(peer_id) {}

template <typename T>
LWWOperation<T> LWWRegister<T>::set(T new_value) {
    metadata_ = {clock_.tick(), clock_.peer_id()};
    value_ = std::move(new_value);
    return {value_, metadata_};
}

template <typename T>
void LWWRegister<T>::apply(const LWWOperation<T>& operation) {
    // Observing happens even when the incoming value loses, so future local
    // writes are causally after every remote timestamp this replica has seen.
    clock_.observe(operation.metadata.timestamp);

    if (is_newer(operation.metadata, metadata_)) {
        value_ = operation.value;
        metadata_ = operation.metadata;
    }
}

template <typename T>
void LWWRegister<T>::merge(const LWWRegister<T>& other) {
    apply({other.value(), other.metadata()});
}

template <typename T>
const T& LWWRegister<T>::value() const {
    return value_;
}

template <typename T>
LWWMetadata LWWRegister<T>::metadata() const {
    return metadata_;
}

// Explicit instantiations. Add a line here for each new T the project uses.
template class LWWRegister<std::string>;

}  // namespace crdt
