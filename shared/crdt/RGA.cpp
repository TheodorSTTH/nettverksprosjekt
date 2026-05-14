#include "RGA.hpp"

#include <utility>

namespace crdt {

RGA::RGA(std::uint32_t peer_id) : peer_id_(peer_id) {}

RGAID RGA::next_id() {
    return RGAID{peer_id_, ++sequence_};
}

void RGA::observe(std::uint32_t remote_sequence) noexcept {
    if (remote_sequence > sequence_) {
        sequence_ = remote_sequence;
    }
}

InsertOp RGA::local_insert(std::string value, RGAID parent_id) {
    const RGAID new_id = next_id();
    integrate(RGANode{value, new_id, parent_id, false});
    return InsertOp{std::move(value), new_id, parent_id};
}

InsertOp RGA::local_insert(char value, RGAID parent_id) {
    return local_insert(std::string(1, value), parent_id);
}

DeleteOp RGA::local_delete(RGAID target_id) {
    auto it = index_of_.find(target_id);
    if (it != index_of_.end()) {
        nodes_[it->second].tombstoned = true;
    }
    return DeleteOp{target_id};
}

void RGA::apply_remote(const InsertOp& op) {
    // Idempotency: already integrated.
    if (index_of_.find(op.id) != index_of_.end()) {
        return;
    }
    // Drop ops that reference an unknown parent. Spec: invalid ops are logged and dropped.
    // (A real implementation could buffer until the parent arrives.)
    if (!op.parent_id.is_root() && index_of_.find(op.parent_id) == index_of_.end()) {
        return;
    }
    observe(op.id.sequence);
    integrate(RGANode{op.value, op.id, op.parent_id, false});
}

void RGA::apply_remote(const DeleteOp& op) {
    auto it = index_of_.find(op.target_id);
    if (it != index_of_.end()) {
        nodes_[it->second].tombstoned = true;
    }
}

std::string RGA::to_string() const {
    std::string out;
    for (const auto& node : nodes_) {
        if (!node.tombstoned) {
            out.append(node.value);
        }
    }
    return out;
}

void RGA::integrate(const RGANode& node) {
    const std::size_t pos = find_insert_position(node.parent_id, node.id);
    nodes_.insert(nodes_.begin() + static_cast<std::ptrdiff_t>(pos), node);

    // Every previously-stored index >= pos shifts right by one.
    for (auto& [id, idx] : index_of_) {
        if (idx >= pos) ++idx;
    }
    index_of_.emplace(node.id, pos);
}

// Linearization rule: among children of P, higher IDs sit closer to P. To
// place new_id under P, walk forward from P and skip past any subtree whose
// root (the first ancestor that is a direct child of P) has ID > new_id.
// Stop at the first subtree where new_id wins, or where we've left P entirely.
std::size_t RGA::find_insert_position(RGAID parent_id, RGAID new_id) const {
    std::size_t j;
    if (parent_id.is_root()) {
        j = 0;
    } else {
        auto it = index_of_.find(parent_id);
        if (it == index_of_.end()) {
            return nodes_.size();  // Defensive: caller should have validated parent.
        }
        j = it->second + 1;
    }

    while (j < nodes_.size()) {
        // Walk up the parent chain from nodes_[j] until we land on a node
        // whose parent is parent_id — that node is the subtree-root containing j.
        RGAID anc_id = nodes_[j].id;
        RGAID anc_parent = nodes_[j].parent_id;
        while (!(anc_parent == parent_id)) {
            auto it = index_of_.find(anc_parent);
            if (it == index_of_.end()) break;  // Hit root or unknown; we've left P's subtree.
            anc_id = anc_parent;
            anc_parent = nodes_[it->second].parent_id;
        }
        if (!(anc_parent == parent_id)) {
            break;  // nodes_[j] is outside parent_id's subtree.
        }

        if (anc_id > new_id) {
            ++j;  // Sibling subtree wins; skip past this node (and naturally through its descendants).
        } else {
            break;  // new_id wins this slot.
        }
    }
    return j;
}

}  // namespace crdt
