#include "crdt/GCounter.hpp"

#include <cstdint>
#include <emscripten/bind.h>
#include <string>

namespace {

void incrementCounter(crdt::GCounter& counter, const std::string& node_id, std::uint32_t amount) {
    counter.increment(node_id, amount);
}

void mergeCounter(crdt::GCounter& counter, const crdt::GCounter& other) {
    counter.merge(other);
}

double counterValue(const crdt::GCounter& counter) {
    return static_cast<double>(counter.value());
}

}  // namespace

EMSCRIPTEN_BINDINGS(crdt_module) {
    using namespace emscripten;

    class_<crdt::GCounter>("GCounter")
        .constructor<>()
        .function("increment", &incrementCounter)
        .function("merge", &mergeCounter)
        .function("value", &counterValue);
}
