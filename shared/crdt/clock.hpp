#pragma once

#include <algorithm>
#include <cstdint>

namespace crdt {

/**
 * @brief Lamport logical clock, used to provide a causal ordering of events across peers.
 *
 * Each peer keeps a single monotonically increasing counter. The clock
 * advances on every local event (tick) and is synchronized with remote
 * timestamps observed in incoming messages (observe). Lamport clocks give
 * a total order on events that respects causality, without the overhead
 * of vector clocks. The cost is that concurrent events are still ordered,
 * just not in a way that reflects causal independence.
 *
 * Used by LWWRegister to timestamp writes, and by the RGA's per-peer
 * sequence numbering in spirit (though that uses a separate counter).
 */
class LamportClock {
public:
    /**
     * @brief Construct a clock for the given peer, starting at timestamp 0.
     */
    explicit LamportClock(std::uint32_t peer_id) : peer_id_(peer_id) {}

    /**
     * @brief Advance the clock for a local event and return the new timestamp.
     */
    std::uint64_t tick() {
        return ++timestamp_;
    }

    /**
     * @brief Observe a timestamp from a remote peer and advance the local clock past it.
     *
     * The new local timestamp is max(local, remote) + 1, which preserves
     * the Lamport happens-before relation across peers.
     */
    void observe(std::uint64_t remote_timestamp) {
        timestamp_ = std::max(timestamp_, remote_timestamp) + 1;
    }

    /**
     * @brief The peer this clock belongs to.
     */
    std::uint32_t peer_id() const {
        return peer_id_;
    }

    /**
     * @brief Current timestamp without advancing the clock.
     */
    std::uint64_t timestamp() const {
        return timestamp_;
    }

private:
    std::uint32_t peer_id_;
    std::uint64_t timestamp_ = 0;
};

}
