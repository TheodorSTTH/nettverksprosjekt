#include "server/document_state.hpp"

#include <fstream>

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
    } catch (...) {
        document_ = crdt::RGA(document_.peer_id());
    }
}

void DocumentState::save() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (snapshot_path_.has_parent_path()) {
        std::filesystem::create_directories(snapshot_path_.parent_path());
    }

    std::ofstream output(snapshot_path_);
    output << protocol::serialize_sync_state(document_).dump(2) << '\n';
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

nlohmann::json DocumentState::snapshot_json() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return protocol::serialize_sync_state(document_);
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

}  // namespace server
