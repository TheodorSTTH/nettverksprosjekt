#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace crdt {

    class GCounter {
        public:
            bool merge_count(const std::string& node_id, std::uint64_t count);
            void merge(const GCounter& other);
            std::uint64_t count_for(const std::string& node_id) const;
            std::uint64_t value() const;
            const std::unordered_map<std::string, std::uint64_t>& counts() const;

        private:
            std::unordered_map<std::string, std::uint64_t> counts_;
    };

}  // namespace crdt
