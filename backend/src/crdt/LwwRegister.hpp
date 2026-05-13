#pragma once

#include <cstdint>

namespace crdt {

template <typename T>
class LwwRegister {
public:
    LwwRegister(T value, std::uint64_t timestamp)
        : value_(value), timestamp_(timestamp) {}

    [[nodiscard]] const T& value() const {
        return value_;
    }

    [[nodiscard]] std::uint64_t timestamp() const {
        return timestamp_;
    }

    bool set(T value, std::uint64_t timestamp) {
        if (timestamp < timestamp_) {
            return false;
        }

        value_ = value;
        timestamp_ = timestamp;
        return true;
    }

private:
    T value_;
    std::uint64_t timestamp_;
};

} // namespace crdt
