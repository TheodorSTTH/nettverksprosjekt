#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace crdt {

    class GCounter {
        public:
            void increment(const std::string& node_id, std::uint64_t amount = 1);
            void merge(const GCounter& other);
            std::uint64_t value() const;

        private:
            std::unordered_map<std::string, std::uint64_t> counts_;
    };


    // class RGA {
    //     public:
    //         void insert(double afterPlace, char value, double place);
    // };



}  // namespace crdt
