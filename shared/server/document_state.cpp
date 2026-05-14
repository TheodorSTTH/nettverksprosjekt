#include "server/document_state.hpp"

#include <fstream>
#include <string>

#include "protocol/message.hpp"

namespace server {

DocumentState::DocumentState(std::uint32_t server_peer_id, std::filesystem::path snapshot_path)
    : document_(server_peer_id), snapshot_path_(std::move(snapshot_path)) {}

void DocumentState::load() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ifstream input(snapshot_path_);
    if (!input) {
        return;
    }

    try {
        nlohmann::json snapshot;
        input >> snapshot;
        document_ = protocol::document_from_snapshot(snapshot, document_.peer_id());
        load_cookie_counter(snapshot);
    } catch (...) {
        document_ = crdt::RGA(document_.peer_id());
        cookie_counter_ = crdt::GCounter();
    }
}

void DocumentState::save() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (snapshot_path_.has_parent_path()) {
        std::filesystem::create_directories(snapshot_path_.parent_path());
    }

    auto snapshot = protocol::serialize_sync_state(document_);
    snapshot["cookie_counter"] = cookie_counter_json();

    std::ofstream output(snapshot_path_);
    output << snapshot.dump(2) << '\n';
}

InsertResult DocumentState::apply_insert(const crdt::InsertOp& operation) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (has_node(operation.id)) {
        return InsertResult{ApplyStatus::Duplicate, operation};
    }

    if (!operation.parent_id.is_root() && !has_node(operation.parent_id)) {
        return InsertResult{ApplyStatus::MissingParent, operation};
    }

    document_.apply_remote(operation);
    return InsertResult{ApplyStatus::Applied, operation};
}

DeleteResult DocumentState::apply_delete(const crdt::DeleteOp& operation) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!has_node(operation.target_id)) {
        return DeleteResult{ApplyStatus::MissingTarget, operation};
    }

    document_.apply_remote(operation);
    return DeleteResult{ApplyStatus::Applied, operation};
}

bool DocumentState::apply_cookie_count(std::uint32_t peer_id, std::uint64_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    return cookie_counter_.merge_count(std::to_string(peer_id), count);
}

nlohmann::json DocumentState::snapshot_json() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto snapshot = protocol::serialize_sync_state(document_);
    snapshot["cookie_counter"] = cookie_counter_json();
    return snapshot;
}

nlohmann::json DocumentState::cookie_counter_state(std::uint32_t peer_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return protocol::serialize_counter_state(
        "cookies",
        cookie_counter_.value(),
        cookie_counter_.count_for(std::to_string(peer_id))
    );
}

std::string DocumentState::visible_text() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return document_.to_string();
}

bool DocumentState::has_node(const crdt::RGAID& id) const {
    for (const auto& node : document_.nodes()) {
        if (node.id == id) {
            return true;
        }
    }

    return false;
}

void DocumentState::load_cookie_counter(const nlohmann::json& snapshot) {
    cookie_counter_ = crdt::GCounter();

    if (!snapshot.contains("cookie_counter") || !snapshot.at("cookie_counter").is_object()) {
        return;
    }

    const auto& cookie_counter = snapshot.at("cookie_counter");
    if (!cookie_counter.contains("counts") || !cookie_counter.at("counts").is_object()) {
        return;
    }

    for (const auto& [node_id, value] : cookie_counter.at("counts").items()) {
        if (value.is_number_unsigned()) {
            cookie_counter_.merge_count(node_id, value.get<std::uint64_t>());
        } else if (value.is_number_integer()) {
            const auto signed_value = value.get<std::int64_t>();
            if (signed_value >= 0) {
                cookie_counter_.merge_count(node_id, static_cast<std::uint64_t>(signed_value));
            }
        }
    }
}

nlohmann::json DocumentState::cookie_counter_json() const {
    nlohmann::json counts = nlohmann::json::object();
    for (const auto& [node_id, count] : cookie_counter_.counts()) {
        counts[node_id] = count;
    }

    return nlohmann::json{{"counts", counts}};
}

}  // namespace server
