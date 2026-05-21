#include "GCounter.hpp"

#include <algorithm>

namespace crdt {

bool GCounter::merge_count(const std::string& node_id, std::uint64_t count) {
    auto& current = counts_[node_id];

    // G-Counter components are absolute monotonically increasing values.
    // Taking max makes retries and duplicate messages idempotent.
    if (count <= current) {
        return false;
    }

    current = count;
    return true;
}

void GCounter::merge(const GCounter& other) {
    for (const auto& [node_id, count] : other.counts_) {
        // Join rule: keep the largest value observed for each replica slot.
        counts_[node_id] = std::max(counts_[node_id], count);
    }
}

std::uint64_t GCounter::count_for(const std::string& node_id) const {
    const auto count = counts_.find(node_id);
    return count == counts_.end() ? 0 : count->second;
}

std::uint64_t GCounter::value() const {
    std::uint64_t total = 0;

    // The user-visible value is derived state: sum of all replica components.
    for (const auto& [_, count] : counts_) {
        total += count;
    }

    return total;
}

const std::unordered_map<std::string, std::uint64_t>& GCounter::counts() const {
    return counts_;
}

}  // namespace crdt
