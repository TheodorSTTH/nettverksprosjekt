#include <cstdint>
#include <string>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "crdt/RGA.hpp"

namespace {

using emscripten::class_;
using emscripten::val;

crdt::RGAID id_from_js(const val& value) {
    return crdt::RGAID{
        value["peer_id"].as<std::uint32_t>(),
        value["sequence"].as<std::uint32_t>(),
    };
}

val id_to_js(const crdt::RGAID& id) {
    val value = val::object();
    value.set("peer_id", id.peer_id);
    value.set("sequence", id.sequence);
    return value;
}

val error_to_js(const std::string& message) {
    val value = val::object();
    value.set("type", std::string("error"));
    value.set("message", message);
    return value;
}

val insert_to_js(const crdt::InsertOp& op, const std::string& type) {
    const std::string character(1, op.value);

    val value = val::object();
    value.set("type", type);
    value.set("value", character);
    value.set("char", character);
    value.set("id", id_to_js(op.id));
    value.set("parent_id", id_to_js(op.parent_id));
    return value;
}

val delete_to_js(const crdt::DeleteOp& op, const std::string& type) {
    val value = val::object();
    value.set("type", type);
    value.set("target_id", id_to_js(op.target_id));
    return value;
}

class WasmRGA {
public:
    explicit WasmRGA(std::uint32_t peer_id) : peer_id_(peer_id), document_(peer_id) {}

    void set_peer_id(std::uint32_t peer_id) {
        if (peer_id_ == peer_id) {
            return;
        }

        const auto nodes = document_.nodes();
        peer_id_ = peer_id;
        document_ = crdt::RGA(peer_id_);
        replay_nodes(nodes);
    }

    void load_snapshot(const val& nodes) {
        document_ = crdt::RGA(peer_id_);

        const auto length = nodes["length"].as<unsigned>();
        for (unsigned index = 0; index < length; ++index) {
            const val node = nodes[index];
            const auto text = node["value"].as<std::string>();
            if (text.size() != 1) {
                continue;
            }

            const auto id = id_from_js(node["id"]);
            const auto parent_id = id_from_js(node["parent_id"]);
            document_.apply_remote(crdt::InsertOp{text[0], id, parent_id});

            if (node["tombstoned"].as<bool>()) {
                document_.apply_remote(crdt::DeleteOp{id});
            }
        }
    }

    val local_insert(const std::string& text, const val& parent_id_value) {
        if (text.size() != 1) {
            return error_to_js("RGA inserts must contain exactly one character");
        }

        const auto parent_id = id_from_js(parent_id_value);
        if (!parent_id.is_root() && !has_node(parent_id)) {
            return error_to_js("insert parent does not exist");
        }

        return insert_to_js(document_.local_insert(text[0], parent_id), "insert");
    }

    val local_delete(const val& target_id_value) {
        const auto target_id = id_from_js(target_id_value);
        if (!has_node(target_id)) {
            return error_to_js("delete target does not exist");
        }

        return delete_to_js(document_.local_delete(target_id), "delete");
    }

    bool apply_remote_insert(const val& operation) {
        const auto id = id_from_js(operation["id"]);
        const auto parent_id = id_from_js(operation["parent_id"]);

        if (has_node(id)) {
            return true;
        }

        if (!parent_id.is_root() && !has_node(parent_id)) {
            return false;
        }

        std::string text;
        if (operation.hasOwnProperty("value")) {
            text = operation["value"].as<std::string>();
        } else {
            text = operation["char"].as<std::string>();
        }

        if (text.size() != 1) {
            return false;
        }

        document_.apply_remote(crdt::InsertOp{text[0], id, parent_id});
        return true;
    }

    bool apply_remote_delete(const val& operation) {
        const auto target_id = id_from_js(operation["target_id"]);
        if (!has_node(target_id)) {
            return false;
        }

        document_.apply_remote(crdt::DeleteOp{target_id});
        return true;
    }

    std::string to_string() const {
        return document_.to_string();
    }

    val visible_ids() const {
        val ids = val::array();
        unsigned visible_index = 0;

        for (const auto& node : document_.nodes()) {
            if (!node.tombstoned) {
                ids.set(visible_index++, id_to_js(node.id));
            }
        }

        return ids;
    }

    val nodes_json() const {
        val nodes = val::array();
        unsigned index = 0;

        for (const auto& node : document_.nodes()) {
            val item = val::object();
            item.set("value", std::string(1, node.value));
            item.set("id", id_to_js(node.id));
            item.set("parent_id", id_to_js(node.parent_id));
            item.set("tombstoned", node.tombstoned);
            nodes.set(index++, item);
        }

        return nodes;
    }

private:
    bool has_node(const crdt::RGAID& id) const {
        for (const auto& node : document_.nodes()) {
            if (node.id == id) {
                return true;
            }
        }

        return false;
    }

    void replay_nodes(const std::vector<crdt::RGANode>& nodes) {
        for (const auto& node : nodes) {
            document_.apply_remote(crdt::InsertOp{node.value, node.id, node.parent_id});
            if (node.tombstoned) {
                document_.apply_remote(crdt::DeleteOp{node.id});
            }
        }
    }

    std::uint32_t peer_id_;
    crdt::RGA document_;
};

}  // namespace

EMSCRIPTEN_BINDINGS(crdt_editor_rga) {
    class_<WasmRGA>("CrdtDocument")
        .constructor<std::uint32_t>()
        .function("setPeerId", &WasmRGA::set_peer_id)
        .function("loadSnapshot", &WasmRGA::load_snapshot)
        .function("localInsert", &WasmRGA::local_insert)
        .function("localDelete", &WasmRGA::local_delete)
        .function("applyRemoteInsert", &WasmRGA::apply_remote_insert)
        .function("applyRemoteDelete", &WasmRGA::apply_remote_delete)
        .function("toString", &WasmRGA::to_string)
        .function("visibleIds", &WasmRGA::visible_ids)
        .function("nodesJson", &WasmRGA::nodes_json);
}
