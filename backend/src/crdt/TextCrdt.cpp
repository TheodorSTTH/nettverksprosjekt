#include "crdt/TextCrdt.hpp"

#include <algorithm>

namespace crdt {

bool TextCrdt::contains(const std::string& id) const {
    if (id.empty()) {
        return true;
    }

    return findCharacter(id) != characters_.end();
}

std::vector<Character>::const_iterator TextCrdt::findCharacter(const std::string& id) const {
    return std::find_if(characters_.begin(), characters_.end(), [&](const Character& character) {
        return character.id == id;
    });
}

std::vector<Character>::iterator TextCrdt::findCharacter(const std::string& id) {
    return std::find_if(characters_.begin(), characters_.end(), [&](const Character& character) {
        return character.id == id;
    });
}

std::vector<Character>::iterator TextCrdt::insertionPoint(
    const std::string& afterId,
    const std::string& beforeId
) {
    if (!beforeId.empty()) {
        auto before = findCharacter(beforeId);
        if (before != characters_.end()) {
            return before;
        }
    }

    if (!afterId.empty()) {
        auto after = findCharacter(afterId);
        if (after != characters_.end()) {
            return std::next(after);
        }
    }

    return characters_.end();
}

std::vector<std::string> TextCrdt::insertText(
    std::string afterId,
    std::string beforeId,
    const std::string& text,
    const std::string& clientId,
    std::uint64_t& sequence
) {
    std::vector<std::string> insertedIds;

    if (!contains(afterId)) {
        afterId = RootId;
    }

    if (!beforeId.empty() && !contains(beforeId)) {
        beforeId.clear();
    }

    auto insertAt = insertionPoint(afterId, beforeId);

    for (unsigned char byte : text) {
        ++sequence;

        Character character;
        character.id = clientId + ":" + std::to_string(sequence);
        character.afterId = afterId;
        character.beforeId = beforeId;
        character.value = std::string(1, static_cast<char>(byte));
        character.sequence = sequence;

        afterId = character.id;
        beforeId.clear();
        insertedIds.push_back(character.id);
        insertAt = characters_.insert(insertAt, std::move(character));
        insertAt = std::next(insertAt);
    }

    return insertedIds;
}

bool TextCrdt::remove(const std::string& id) {
    auto character = findCharacter(id);

    if (character != characters_.end() && !character->tombstone) {
        character->tombstone = true;
        return true;
    }

    return false;
}

bool TextCrdt::removeMany(const std::vector<std::string>& ids) {
    bool changed = false;

    for (const std::string& id : ids) {
        changed = remove(id) || changed;
    }

    return changed;
}

std::vector<VisibleCharacter> TextCrdt::visibleCharacters() const {
    std::vector<VisibleCharacter> result;

    for (const Character& character : characters_) {
        if (!character.tombstone) {
            result.push_back({character.id, character.value});
        }
    }

    return result;
}

std::string TextCrdt::render() const {
    std::string output;

    for (const VisibleCharacter& character : visibleCharacters()) {
        output += character.value;
    }

    return output;
}

std::uint64_t TextCrdt::maxSequence() const {
    std::uint64_t max = 0;

    for (const Character& character : characters_) {
        max = std::max(max, character.sequence);
    }

    return max;
}

nlohmann::json TextCrdt::toJson() const {
    nlohmann::json characters = nlohmann::json::array();

    for (const Character& character : characters_) {
        characters.push_back({
            {"id", character.id},
            {"afterId", character.afterId},
            {"beforeId", character.beforeId},
            {"value", character.value},
            {"tombstone", character.tombstone},
            {"sequence", character.sequence},
        });
    }

    return characters;
}

TextCrdt TextCrdt::fromJson(const nlohmann::json& json) {
    TextCrdt crdt;

    if (!json.is_array()) {
        return crdt;
    }

    for (const auto& item : json) {
        Character character;
        character.id = item.value("id", "");
        character.afterId = item.value("afterId", "");
        character.beforeId = item.value("beforeId", "");
        character.value = item.value("value", "");
        character.tombstone = item.value("tombstone", false);
        character.sequence = item.value("sequence", 0ULL);

        if (!character.id.empty()) {
            crdt.characters_.push_back(std::move(character));
        }
    }

    return crdt;
}

} // namespace crdt
