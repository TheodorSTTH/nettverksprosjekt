#pragma once

#include <cstdint>
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

inline bool is_newer(const LWWMetadata& candidate, const LWWMetadata& current) {
    if (candidate.timestamp != current.timestamp) {
        return candidate.timestamp > current.timestamp;
    }

    return candidate.peer_id > current.peer_id;
}

template <typename T>
struct LWWOperation {
    T value;
    LWWMetadata metadata;
};

template <typename T>
class LWWRegister {
public:
    explicit LWWRegister(std::uint32_t peer_id, T initial_value = T{})
        : value_(std::move(initial_value)), metadata_{0, peer_id}, clock_(peer_id) {}

    LWWOperation<T> set(T new_value) {
        metadata_ = {clock_.tick(), clock_.peer_id()};
        value_ = std::move(new_value);
        return {value_, metadata_};
    }

    void apply(const LWWOperation<T>& operation) {
        clock_.observe(operation.metadata.timestamp);

        if (is_newer(operation.metadata, metadata_)) {
            value_ = operation.value;
            metadata_ = operation.metadata;
        }
    }

    void merge(const LWWRegister<T>& other) {
        apply({other.value(), other.metadata()});
    }

    const T& value() const {
        return value_;
    }

    LWWMetadata metadata() const {
        return metadata_;
    }

private:
    T value_;
    LWWMetadata metadata_;
    LamportClock clock_;
};

}
