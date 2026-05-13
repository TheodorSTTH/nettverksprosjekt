#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace crdt {

struct Character {
    std::string id;
    std::string afterId;
    std::string beforeId;
    std::string value;
    bool tombstone = false;
    std::uint64_t sequence = 0;
};

struct VisibleCharacter {
    std::string id;
    std::string value;
};

class TextCrdt {
public:
    static constexpr const char* RootId = "";

    [[nodiscard]] bool contains(const std::string& id) const;
    [[nodiscard]] std::string render() const;
    [[nodiscard]] std::vector<VisibleCharacter> visibleCharacters() const;
    [[nodiscard]] std::uint64_t maxSequence() const;
    [[nodiscard]] nlohmann::json toJson() const;

    static TextCrdt fromJson(const nlohmann::json& json);

    std::vector<std::string> insertText(
        std::string afterId,
        std::string beforeId,
        const std::string& text,
        const std::string& clientId,
        std::uint64_t& sequence
    );

    bool remove(const std::string& id);
    bool removeMany(const std::vector<std::string>& ids);

private:
    [[nodiscard]] std::vector<Character>::iterator insertionPoint(
        const std::string& afterId,
        const std::string& beforeId
    );
    [[nodiscard]] std::vector<Character>::const_iterator findCharacter(const std::string& id) const;
    [[nodiscard]] std::vector<Character>::iterator findCharacter(const std::string& id);

    std::vector<Character> characters_;
};

} // namespace crdt
