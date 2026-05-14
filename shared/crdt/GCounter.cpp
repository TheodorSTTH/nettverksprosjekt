#include "GCounter.hpp"

#include <algorithm>

namespace crdt {

void GCounter::increment(const std::string& node_id, std::uint64_t amount) {
    counts_[node_id] += amount;
}

void GCounter::merge(const GCounter& other) {
    for (const auto& [node_id, count] : other.counts_) {
        counts_[node_id] = std::max(counts_[node_id], count);
    }
}

std::uint64_t GCounter::value() const {
    std::uint64_t total = 0;

    for (const auto& [_, count] : counts_) {
        total += count;
    }

    return total;
}

}  // namespace crdt
