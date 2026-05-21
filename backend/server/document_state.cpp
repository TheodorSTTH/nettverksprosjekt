#include "server/document_state.hpp"

#include <fstream>
#include <string>

#include "protocol/message.hpp"

namespace server {

DocumentState::DocumentState(std::uint32_t server_peer_id, std::filesystem::path snapshot_path)
    : document_(server_peer_id),
      title_(server_peer_id, std::string("Untitled")),
      snapshot_path_(std::move(snapshot_path)) {}

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
        load_title(snapshot);
    } catch (...) {
        document_ = crdt::RGA(document_.peer_id());
        cookie_counter_ = crdt::GCounter();
        title_ = crdt::LWWRegister<std::string>(document_.peer_id(), std::string("Untitled"));
    }
}

void DocumentState::save() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (snapshot_path_.has_parent_path()) {
        std::filesystem::create_directories(snapshot_path_.parent_path());
    }

    auto snapshot = protocol::serialize_sync_state(document_, title_);
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

bool DocumentState::apply_cookie_count(const std::string& replica_id, std::uint64_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    return cookie_counter_.merge_count(replica_id, count);
}

void DocumentState::apply_set_title(const crdt::LWWOperation<std::string>& operation) {
    std::lock_guard<std::mutex> lock(mutex_);
    title_.apply(operation);
}

nlohmann::json DocumentState::snapshot_json() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto snapshot = protocol::serialize_sync_state(document_, title_);
    snapshot["cookie_counter"] = cookie_counter_json();
    return snapshot;
}

nlohmann::json DocumentState::title_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return protocol::serialize_title_state(title_);
}

nlohmann::json DocumentState::cookie_counter_state(const std::string& replica_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return protocol::serialize_counter_state(
        "cookies",
        cookie_counter_.value(),
        cookie_counter_.count_for(replica_id),
        replica_id
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

void DocumentState::load_title(const nlohmann::json& snapshot) {
    title_ = crdt::LWWRegister<std::string>(document_.peer_id(), std::string("Untitled"));

    if (!snapshot.contains("title") || !snapshot.at("title").is_object()) {
        return;
    }

    const auto& title = snapshot.at("title");
    if (!title.contains("value") || !title.at("value").is_string() ||
        !title.contains("timestamp") || !title.contains("peer_id")) {
        return;
    }

    std::uint64_t timestamp = 0;
    if (title.at("timestamp").is_number_unsigned()) {
        timestamp = title.at("timestamp").get<std::uint64_t>();
    } else if (title.at("timestamp").is_number_integer()) {
        const auto signed_ts = title.at("timestamp").get<std::int64_t>();
        if (signed_ts < 0) {
            return;
        }
        timestamp = static_cast<std::uint64_t>(signed_ts);
    } else {
        return;
    }

    std::uint32_t peer_id = 0;
    if (title.at("peer_id").is_number_unsigned()) {
        peer_id = static_cast<std::uint32_t>(title.at("peer_id").get<std::uint64_t>());
    } else if (title.at("peer_id").is_number_integer()) {
        const auto signed_pid = title.at("peer_id").get<std::int64_t>();
        if (signed_pid < 0) {
            return;
        }
        peer_id = static_cast<std::uint32_t>(signed_pid);
    } else {
        return;
    }

    title_.apply(crdt::LWWOperation<std::string>{
        title.at("value").get<std::string>(),
        crdt::LWWMetadata{timestamp, peer_id},
    });
}

nlohmann::json DocumentState::cookie_counter_json() const {
    nlohmann::json counts = nlohmann::json::object();
    for (const auto& [node_id, count] : cookie_counter_.counts()) {
        counts[node_id] = count;
    }

    return nlohmann::json{{"counts", counts}};
}

}  // namespace server
