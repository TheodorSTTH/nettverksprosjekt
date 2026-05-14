#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "crdt/RGA.hpp"

namespace server {

enum class ApplyStatus {
    Applied,
    MissingParent,
    MissingTarget,
    Duplicate,
};

struct InsertResult {
    ApplyStatus status = ApplyStatus::Applied;
    crdt::InsertOp operation{};
};

struct DeleteResult {
    ApplyStatus status = ApplyStatus::Applied;
    crdt::DeleteOp operation{};
};

class DocumentState {
public:
    DocumentState(std::uint32_t server_peer_id, std::filesystem::path snapshot_path);

    void load();
    void save() const;

    InsertResult apply_insert(const crdt::InsertOp& operation);
    DeleteResult apply_delete(const crdt::DeleteOp& operation);

    nlohmann::json snapshot_json() const;
    std::string visible_text() const;

private:
    bool has_node(const crdt::RGAID& id) const;

    mutable std::mutex mutex_;
    crdt::RGA document_;
    std::filesystem::path snapshot_path_;
};

}  // namespace server
