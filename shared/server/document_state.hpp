#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "crdt/GCounter.hpp"
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
    bool apply_cookie_count(std::uint32_t peer_id, std::uint64_t count);

    nlohmann::json snapshot_json() const;
    nlohmann::json cookie_counter_state(std::uint32_t peer_id) const;
    std::string visible_text() const;

private:
    bool has_node(const crdt::RGAID& id) const;
    void load_cookie_counter(const nlohmann::json& snapshot);
    nlohmann::json cookie_counter_json() const;

    mutable std::mutex mutex_;
    crdt::RGA document_;
    crdt::GCounter cookie_counter_;
    std::filesystem::path snapshot_path_;
};

}  // namespace server
