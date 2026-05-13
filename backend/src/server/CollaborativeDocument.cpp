#include "server/CollaborativeDocument.hpp"

#include <algorithm>
#include <vector>

namespace server {

namespace {

std::string jsonString(const nlohmann::json& json, const char* key, std::string fallback = "") {
    if (!json.contains(key) || !json.at(key).is_string()) {
        return fallback;
    }

    return json.at(key).get<std::string>();
}

std::vector<std::string> jsonStringArray(const nlohmann::json& json) {
    std::vector<std::string> values;

    if (!json.is_array()) {
        return values;
    }

    for (const auto& item : json) {
        if (item.is_string()) {
            values.push_back(item.get<std::string>());
        }
    }

    return values;
}

} // namespace

CollaborativeDocument::CollaborativeDocument()
    : fontSize_(16, 0) {}

void CollaborativeDocument::bumpRevision() {
    ++revision_;
    ++clock_;
}

void CollaborativeDocument::updatePresence(const nlohmann::json& message) {
    const std::string clientId = jsonString(message, "clientId");

    if (clientId.empty()) {
        return;
    }

    UserPresence& user = users_[clientId];
    user.clientId = clientId;
    user.userName = jsonString(message, "userName", "Anonymous");

    if (message.contains("cursor") && message.at("cursor").is_object()) {
        user.cursorAfterId = jsonString(message.at("cursor"), "afterId");
    }
}

void CollaborativeDocument::removePresence(const std::string& clientId) {
    if (!clientId.empty()) {
        users_.erase(clientId);
    }
}

bool CollaborativeDocument::applyEditBatch(const nlohmann::json& message) {
    updatePresence(message);

    bool changed = false;
    const std::string clientId = jsonString(message, "clientId", "client");

    if (!message.contains("ops") || !message.at("ops").is_array()) {
        return false;
    }

    for (const auto& op : message.at("ops")) {
        const std::string type = jsonString(op, "type");

        if (type == "insert") {
            const std::string afterId = jsonString(op, "afterId");
            const std::string beforeId = jsonString(op, "beforeId");
            const std::string text = jsonString(op, "text");

            if (!text.empty()) {
                text_.insertText(afterId, beforeId, text, clientId, nextSequence_);
                changed = true;
            }
        } else if (type == "delete" && op.contains("ids")) {
            changed = text_.removeMany(jsonStringArray(op.at("ids"))) || changed;
        }
    }

    if (changed) {
        bumpRevision();
    }

    return changed;
}

bool CollaborativeDocument::setFontSize(const nlohmann::json& message) {
    updatePresence(message);

    if (!message.contains("value") || !message.at("value").is_number_integer()) {
        return false;
    }

    const int value = std::clamp(message.at("value").get<int>(), 10, 48);
    const bool changed = fontSize_.value() != value;

    ++clock_;
    fontSize_.set(value, clock_);

    if (changed) {
        ++revision_;
    }

    return changed;
}

nlohmann::json CollaborativeDocument::snapshot() const {
    nlohmann::json characters = nlohmann::json::array();
    for (const crdt::VisibleCharacter& character : text_.visibleCharacters()) {
        characters.push_back({
            {"id", character.id},
            {"value", character.value},
        });
    }

    nlohmann::json users = nlohmann::json::array();
    for (const auto& [_, user] : users_) {
        users.push_back({
            {"clientId", user.clientId},
            {"userName", user.userName},
            {"cursor", {{"afterId", user.cursorAfterId}}},
        });
    }

    return {
        {"type", "document:snapshot"},
        {"revision", revision_},
        {"text", text_.render()},
        {"chars", characters},
        {"fontSize", fontSize_.value()},
        {"users", users},
    };
}

nlohmann::json CollaborativeDocument::toPersistedJson() const {
    return {
        {"revision", revision_},
        {"clock", clock_},
        {"nextSequence", nextSequence_},
        {"fontSize", {
            {"value", fontSize_.value()},
            {"timestamp", fontSize_.timestamp()},
        }},
        {"characters", text_.toJson()},
    };
}

CollaborativeDocument CollaborativeDocument::fromPersistedJson(const nlohmann::json& json) {
    CollaborativeDocument document;

    if (!json.is_object()) {
        return document;
    }

    document.revision_ = json.value("revision", 0ULL);
    document.clock_ = json.value("clock", 0ULL);
    document.nextSequence_ = json.value("nextSequence", 0ULL);

    if (json.contains("characters")) {
        document.text_ = crdt::TextCrdt::fromJson(json.at("characters"));
        document.nextSequence_ = std::max(document.nextSequence_, document.text_.maxSequence());
    }

    if (json.contains("fontSize") && json.at("fontSize").is_object()) {
        const auto& font = json.at("fontSize");
        document.fontSize_.set(font.value("value", 16), font.value("timestamp", 0ULL));
    }

    return document;
}

} // namespace server
