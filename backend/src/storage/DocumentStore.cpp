#include "storage/DocumentStore.hpp"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace storage {

DocumentStore::DocumentStore(std::filesystem::path path)
    : path_(std::move(path)) {}

server::CollaborativeDocument DocumentStore::load() const {
    if (!std::filesystem::exists(path_)) {
        return {};
    }

    std::ifstream input(path_);
    if (!input) {
        std::cerr << "Could not open document store at " << path_ << '\n';
        return {};
    }

    try {
        nlohmann::json json;
        input >> json;
        return server::CollaborativeDocument::fromPersistedJson(json);
    } catch (const std::exception& error) {
        std::cerr << "Could not load document store: " << error.what() << '\n';
        return {};
    }
}

void DocumentStore::save(const server::CollaborativeDocument& document) const {
    std::filesystem::create_directories(path_.parent_path());

    std::ofstream output(path_);
    if (!output) {
        std::cerr << "Could not write document store at " << path_ << '\n';
        return;
    }

    output << document.toPersistedJson().dump(2);
}

} // namespace storage
