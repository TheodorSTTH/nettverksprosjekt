#include <catch2/catch_test_macros.hpp>

#include "crdt/RGA.hpp"

using namespace crdt;

// Insert a string sequentially into a fresh RGA starting from root.
// Returns the ID of the last inserted character.
static RGAID insert_string(RGA& rga, const std::string& text) {
    RGAID parent = RGAID::root();
    for (char c : text) {
        auto op = rga.local_insert(c, parent);
        parent = op.id;
    }
    return parent;
}

// ─── Basic operations ─────────────────────────────────────────────────────────

TEST_CASE("Insert into empty document", "[rga][basic]") {
    RGA rga(1);
    rga.local_insert('A', RGAID::root());
    REQUIRE(rga.to_string() == "A");
}

TEST_CASE("Sequential inserts build correct string", "[rga][basic]") {
    RGA rga(1);
    insert_string(rga, "hello");
    REQUIRE(rga.to_string() == "hello");
}

TEST_CASE("Sequential Unicode inserts build correct string", "[rga][unicode]") {
    RGA rga(1);
    auto ae = rga.local_insert("æ", RGAID::root());
    auto oe = rga.local_insert("ø", ae.id);
    rga.local_insert("å", oe.id);

    REQUIRE(rga.to_string() == "æøå");
    REQUIRE(rga.nodes().size() == 3);
}

TEST_CASE("Delete removes character from visible text", "[rga][basic]") {
    RGA rga(1);
    auto op = rga.local_insert('X', RGAID::root());
    rga.local_delete(op.id);
    REQUIRE(rga.to_string() == "");
}

TEST_CASE("Delete tombstones node but keeps it in node list", "[rga][basic]") {
    RGA rga(1);
    auto op = rga.local_insert('X', RGAID::root());
    rga.local_delete(op.id);

    REQUIRE(rga.nodes().size() == 1);
    REQUIRE(rga.nodes()[0].tombstoned == true);
}

TEST_CASE("to_string skips all tombstoned nodes", "[rga][basic]") {
    RGA rga(1);
    insert_string(rga, "abc");

    // tombstone the middle character
    const RGAID mid_id = rga.nodes()[1].id;
    rga.local_delete(mid_id);

    REQUIRE(rga.to_string() == "ac");
}

// ─── Edge cases ───────────────────────────────────────────────────────────────

TEST_CASE("Insert at beginning of non-empty document", "[rga][edge]") {
    RGA rga(1);
    insert_string(rga, "bc");

    // prepend 'a' using root as parent
    rga.local_insert('a', RGAID::root());
    REQUIRE(rga.to_string() == "abc");
}

TEST_CASE("Insert after last character", "[rga][edge]") {
    RGA rga(1);
    const RGAID last = insert_string(rga, "ab");
    rga.local_insert('c', last);
    REQUIRE(rga.to_string() == "abc");
}

TEST_CASE("Delete first character", "[rga][edge]") {
    RGA rga(1);
    insert_string(rga, "abc");
    rga.local_delete(rga.nodes()[0].id);
    REQUIRE(rga.to_string() == "bc");
}

TEST_CASE("Delete last character", "[rga][edge]") {
    RGA rga(1);
    insert_string(rga, "abc");
    rga.local_delete(rga.nodes()[2].id);
    REQUIRE(rga.to_string() == "ab");
}

TEST_CASE("Insert after a tombstoned node still works", "[rga][edge]") {
    RGA rga(1);
    auto op_a = rga.local_insert('a', RGAID::root());
    rga.local_delete(op_a.id);           // tombstone 'a'
    rga.local_insert('b', op_a.id);      // insert 'b' after the tombstone
    REQUIRE(rga.to_string() == "b");
}

TEST_CASE("Operations on empty document do not crash", "[rga][edge]") {
    RGA rga(1);
    // delete with unknown id — should silently no-op
    rga.local_delete(RGAID{42, 42});
    REQUIRE(rga.to_string() == "");
}

// ─── Idempotency ──────────────────────────────────────────────────────────────

TEST_CASE("Applying a remote insert twice is a no-op", "[rga][idempotency]") {
    RGA rga1(1);
    RGA rga2(2);

    auto op = rga1.local_insert('X', RGAID::root());
    rga2.apply_remote(op);
    rga2.apply_remote(op);  // second application must not duplicate the node

    REQUIRE(rga2.to_string() == "X");
    REQUIRE(rga2.nodes().size() == 1);
}

TEST_CASE("Applying a remote delete twice is a no-op", "[rga][idempotency]") {
    RGA rga1(1);
    RGA rga2(2);

    auto ins = rga1.local_insert('X', RGAID::root());
    auto del = rga1.local_delete(ins.id);

    rga2.apply_remote(ins);
    rga2.apply_remote(del);
    rga2.apply_remote(del);  // second delete must not crash or change state

    REQUIRE(rga2.to_string() == "");
}

TEST_CASE("Concurrent deletes of the same node converge", "[rga][idempotency]") {
    RGA rga1(1);
    RGA rga2(2);

    auto ins = rga1.local_insert('X', RGAID::root());
    rga2.apply_remote(ins);

    auto del1 = rga1.local_delete(ins.id);
    auto del2 = rga2.local_delete(ins.id);

    rga1.apply_remote(del2);
    rga2.apply_remote(del1);

    REQUIRE(rga1.to_string() == rga2.to_string());
    REQUIRE(rga1.to_string() == "");
}

// ─── Convergence (the critical CRDT property) ────────────────────────────────

TEST_CASE("Concurrent inserts at same parent converge regardless of arrival order", "[rga][convergence]") {
    // Both peers start from an identical base document.
    RGA base(0);
    insert_string(base, "ac");
    const RGAID id_a = base.nodes()[0].id;  // after 'a'

    // Replay base state onto both replicas.
    RGA r1(1), r2(2);
    for (const auto& node : base.nodes()) {
        r1.apply_remote(InsertOp{node.value, node.id, node.parent_id});
        r2.apply_remote(InsertOp{node.value, node.id, node.parent_id});
    }

    // Peer 1 inserts 'b' after 'a'; peer 2 inserts 'B' after 'a' concurrently.
    auto op1 = r1.local_insert('b', id_a);
    auto op2 = r2.local_insert('B', id_a);

    // Apply in opposite orders.
    r1.apply_remote(op2);
    r2.apply_remote(op1);

    REQUIRE(r1.to_string() == r2.to_string());
}

TEST_CASE("Concurrent Unicode inserts converge regardless of arrival order", "[rga][unicode][convergence]") {
    RGA r1(1), r2(2);

    auto op1 = r1.local_insert("æ", RGAID::root());
    auto op2 = r2.local_insert("ø", RGAID::root());

    r1.apply_remote(op2);
    r2.apply_remote(op1);

    REQUIRE(r1.to_string() == r2.to_string());
}

TEST_CASE("Three-way concurrent inserts all converge", "[rga][convergence]") {
    RGA r1(1), r2(2), r3(3);

    auto op1 = r1.local_insert('X', RGAID::root());
    auto op2 = r2.local_insert('Y', RGAID::root());
    auto op3 = r3.local_insert('Z', RGAID::root());

    // Every permutation of applying the other two ops.
    r1.apply_remote(op2); r1.apply_remote(op3);
    r2.apply_remote(op3); r2.apply_remote(op1);
    r3.apply_remote(op1); r3.apply_remote(op2);

    REQUIRE(r1.to_string() == r2.to_string());
    REQUIRE(r2.to_string() == r3.to_string());
}

TEST_CASE("Insert then remote delete converges", "[rga][convergence]") {
    RGA r1(1), r2(2);

    auto ins = r1.local_insert('X', RGAID::root());
    r2.apply_remote(ins);

    // r2 deletes while r1 inserts 'Y' after 'X' concurrently.
    auto del  = r2.local_delete(ins.id);
    auto ins2 = r1.local_insert('Y', ins.id);

    r1.apply_remote(del);
    r2.apply_remote(ins2);

    REQUIRE(r1.to_string() == r2.to_string());
}

// ─── Commutativity ───────────────────────────────────────────────────────────

TEST_CASE("Remote ops are commutative: A then B == B then A", "[rga][commutativity]") {
    RGA r1(1), r2(2);

    auto op_a = r1.local_insert('A', RGAID::root());
    auto op_b = r2.local_insert('B', RGAID::root());

    RGA recv1(3), recv2(4);
    recv1.apply_remote(op_a); recv1.apply_remote(op_b);
    recv2.apply_remote(op_b); recv2.apply_remote(op_a);

    REQUIRE(recv1.to_string() == recv2.to_string());
}

// ─── State sync round-trip ───────────────────────────────────────────────────

TEST_CASE("New replica seeded from nodes() produces identical to_string", "[rga][sync]") {
    RGA source(1);
    insert_string(source, "hello");
    source.local_delete(source.nodes()[1].id);  // delete 'e' → "hllo"

    // Simulate sync_state: replay all nodes onto a fresh replica.
    RGA replica(2);
    for (const auto& node : source.nodes()) {
        replica.apply_remote(InsertOp{node.value, node.id, node.parent_id});
        if (node.tombstoned) {
            replica.apply_remote(DeleteOp{node.id});
        }
    }

    REQUIRE(replica.to_string() == source.to_string());
}
