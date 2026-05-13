#include "crdt/TextCrdt.hpp"
#include "server/CollaborativeDocument.hpp"

#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    {
        crdt::TextCrdt text;
        std::uint64_t sequence = 0;
        const auto ids = text.insertText(crdt::TextCrdt::RootId, "", "abc", "alice", sequence);

        require(text.render() == "abc", "initial insert should render abc");
        require(ids.size() == 3, "initial insert should create three ids");

        text.remove(ids[1]);
        require(text.render() == "ac", "delete should hide the removed character");

        text.insertText(ids[0], ids[2], "x", "bob", sequence);
        require(text.render() == "axc", "middle insert should render between anchors");

        text.insertText(ids[0], "bob:4", "y", "carol", sequence);
        require(text.render() == "ayxc", "second middle insert should honor beforeId");
    }

    {
        crdt::TextCrdt left;
        std::uint64_t sequence = 0;
        left.insertText(crdt::TextCrdt::RootId, "", "A", "alice", sequence);
        left.insertText(crdt::TextCrdt::RootId, "alice:1", "B", "bob", sequence);

        const auto serialized = left.toJson();
        const auto restored = crdt::TextCrdt::fromJson(serialized);

        require(left.render() == restored.render(), "serialized CRDT should restore render order");
    }

    {
        server::CollaborativeDocument document;

        nlohmann::json join = {
            {"type", "join"},
            {"clientId", "alice"},
            {"userName", "Alice"},
            {"cursor", {{"afterId", ""}}},
        };
        document.updatePresence(join);

        nlohmann::json batch = {
            {"type", "edit:batch"},
            {"clientId", "alice"},
            {"userName", "Alice"},
            {"ops", nlohmann::json::array({
                nlohmann::json::object({{"type", "insert"}, {"afterId", ""}, {"beforeId", ""}, {"text", "hello"}}),
            })},
            {"cursor", {{"afterId", ""}}},
        };

        require(document.applyEditBatch(batch), "edit batch should be applied");
        require(document.snapshot().at("text").get<std::string>() == "hello", "document should render hello");

        nlohmann::json font = {
            {"type", "font-size:set"},
            {"clientId", "alice"},
            {"value", 22},
        };

        require(document.setFontSize(font), "font size update should be applied");
        require(document.snapshot().at("fontSize").get<int>() == 22, "font size should be 22");
    }

    {
        server::CollaborativeDocument document;

        nlohmann::json first = {
            {"type", "edit:batch"},
            {"clientId", "alice"},
            {"userName", "Alice"},
            {"ops", nlohmann::json::array({
                nlohmann::json::object({{"type", "insert"}, {"afterId", ""}, {"beforeId", ""}, {"text", "ac"}}),
            })},
        };

        require(document.applyEditBatch(first), "first edit batch should be applied");

        const auto chars = document.snapshot().at("chars");
        require(chars.size() == 2, "first batch should create two visible characters");
        nlohmann::json middle = {
            {"type", "edit:batch"},
            {"clientId", "bob"},
            {"userName", "Bob"},
            {"ops", nlohmann::json::array({
                nlohmann::json::object({
                    {"type", "insert"},
                    {"afterId", chars.at(0).at("id").get<std::string>()},
                    {"beforeId", chars.at(1).at("id").get<std::string>()},
                    {"text", "b"}
                }),
            })},
        };

        require(document.applyEditBatch(middle), "middle edit batch should be applied");
        require(document.snapshot().at("text").get<std::string>() == "abc", "middle insert should produce abc");
    }

    return 0;
}
