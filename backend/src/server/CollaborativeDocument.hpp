#pragma once

#include "crdt/LwwRegister.hpp"
#include "crdt/TextCrdt.hpp"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace server {

struct UserPresence {
    std::string clientId;
    std::string userName;
    std::string cursorAfterId;
};

class CollaborativeDocument {
public:
    CollaborativeDocument();

    [[nodiscard]] nlohmann::json snapshot() const;
    [[nodiscard]] nlohmann::json toPersistedJson() const;

    static CollaborativeDocument fromPersistedJson(const nlohmann::json& json);

    bool applyEditBatch(const nlohmann::json& message);
    bool setFontSize(const nlohmann::json& message);
    void updatePresence(const nlohmann::json& message);
    void removePresence(const std::string& clientId);

private:
    void bumpRevision();

    crdt::TextCrdt text_;
    crdt::LwwRegister<int> fontSize_;
    std::unordered_map<std::string, UserPresence> users_;
    std::uint64_t revision_ = 0;
    std::uint64_t clock_ = 0;
    std::uint64_t nextSequence_ = 0;
};

} // namespace server
