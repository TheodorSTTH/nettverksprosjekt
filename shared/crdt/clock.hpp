#pragma once

#include <algorithm>
#include <cstdint>

namespace crdt {

class LamportClock {
public:
    explicit LamportClock(std::uint32_t peer_id) : peer_id_(peer_id) {}

    std::uint64_t tick() {
        return ++timestamp_;
    }

    void observe(std::uint64_t remote_timestamp) {
        timestamp_ = std::max(timestamp_, remote_timestamp) + 1;
    }

    std::uint32_t peer_id() const {
        return peer_id_;
    }

    std::uint64_t timestamp() const {
        return timestamp_;
    }

private:
    std::uint32_t peer_id_;
    std::uint64_t timestamp_ = 0;
};

}
