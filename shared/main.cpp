#include "crdt/GCounter.hpp"

#include <iostream>

int main() {
    crdt::GCounter node_a;
    crdt::GCounter node_b;

    node_a.increment("node-a", 2);
    node_b.increment("node-b", 3);

    node_a.merge(node_b);

    std::cout << "Merged GCounter value: " << node_a.value() << std::endl;
}
