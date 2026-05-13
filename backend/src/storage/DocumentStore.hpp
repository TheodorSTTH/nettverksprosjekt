#pragma once

#include "server/CollaborativeDocument.hpp"

#include <filesystem>

namespace storage {

class DocumentStore {
public:
    explicit DocumentStore(std::filesystem::path path);

    [[nodiscard]] server::CollaborativeDocument load() const;
    void save(const server::CollaborativeDocument& document) const;

private:
    std::filesystem::path path_;
};

} // namespace storage
