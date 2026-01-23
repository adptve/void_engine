// void_ir Snapshot tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/ir/ir.hpp>

using namespace void_ir;

// =============================================================================
// SnapshotId Tests
// =============================================================================

TEST_CASE("SnapshotId", "[ir][snapshot]") {
    SECTION("default is invalid") {
        SnapshotId id;
        REQUIRE_FALSE(id.is_valid());
    }

    SECTION("explicit construction") {
        SnapshotId id(42);
        REQUIRE(id.is_valid());
        REQUIRE(id.value == 42);
    }

    SECTION("comparison") {
        SnapshotId a(1);
        SnapshotId b(1);
        SnapshotId c(2);

        REQUIRE(a == b);
        REQUIRE(a != c);
        REQUIRE(a < c);
    }
}

// =============================================================================
// EntitySnapshot Tests
// =============================================================================

TEST_CASE("EntitySnapshot", "[ir][snapshot]") {
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("construction") {
        EntitySnapshot snap;
        snap.entity = entity;
        snap.name = "Player";
        snap.enabled = true;

        REQUIRE(snap.entity == entity);
        REQUIRE(snap.name == "Player");
    }

    SECTION("components") {
        EntitySnapshot snap;
        snap.entity = entity;
        snap.components["Health"] = Value(100);
        snap.components["Position"] = Value(Vec3{0, 0, 0});

        REQUIRE(snap.has_component("Health"));
        REQUIRE_FALSE(snap.has_component("Unknown"));
        REQUIRE(snap.get_component("Health")->as_int() == 100);
        REQUIRE(snap.get_component("Unknown") == nullptr);
    }

    SECTION("clone") {
        EntitySnapshot snap;
        snap.entity = entity;
        snap.name = "Test";
        snap.components["Value"] = Value(42);

        auto cloned = snap.clone();
        REQUIRE(cloned.entity == snap.entity);
        REQUIRE(cloned.name == snap.name);
        REQUIRE(cloned.has_component("Value"));
    }
}

// =============================================================================
// Snapshot Tests
// =============================================================================

TEST_CASE("Snapshot", "[ir][snapshot]") {
    NamespaceId ns(0);
    SnapshotId snap_id(0);

    SECTION("construction") {
        Snapshot snap(snap_id, ns);

        REQUIRE(snap.id() == snap_id);
        REQUIRE(snap.namespace_id() == ns);
        REQUIRE(snap.entity_count() == 0);
    }

    SECTION("description") {
        Snapshot snap(snap_id, ns);
        snap.set_description("Initial state");

        REQUIRE(snap.description() == "Initial state");
    }

    SECTION("add and get entities") {
        Snapshot snap(snap_id, ns);

        EntityRef entity(ns, 1);
        EntitySnapshot entity_snap;
        entity_snap.entity = entity;
        entity_snap.name = "Player";

        snap.add_entity(std::move(entity_snap));

        REQUIRE(snap.entity_count() == 1);
        REQUIRE(snap.get_entity(entity) != nullptr);
        REQUIRE(snap.get_entity(entity)->name == "Player");
    }

    SECTION("remove entity") {
        Snapshot snap(snap_id, ns);

        EntityRef entity(ns, 1);
        EntitySnapshot entity_snap;
        entity_snap.entity = entity;

        snap.add_entity(std::move(entity_snap));
        snap.remove_entity(entity);

        REQUIRE(snap.entity_count() == 0);
        REQUIRE(snap.get_entity(entity) == nullptr);
    }

    SECTION("layers") {
        Snapshot snap(snap_id, ns);

        LayerSnapshot layer;
        layer.layer = LayerId(0);
        layer.name = "Default";
        layer.order = 0;

        snap.add_layer(std::move(layer));

        REQUIRE(snap.get_layer(LayerId(0)) != nullptr);
        REQUIRE(snap.get_layer(LayerId(0))->name == "Default");
    }

    SECTION("clone") {
        Snapshot snap(snap_id, ns);
        snap.set_description("Test");

        EntityRef entity(ns, 1);
        EntitySnapshot entity_snap;
        entity_snap.entity = entity;
        entity_snap.name = "Test Entity";
        snap.add_entity(std::move(entity_snap));

        auto cloned = snap.clone();

        REQUIRE(cloned.id() == snap.id());
        REQUIRE(cloned.description() == snap.description());
        REQUIRE(cloned.entity_count() == snap.entity_count());
    }
}

// =============================================================================
// SnapshotDelta Tests
// =============================================================================

TEST_CASE("SnapshotDelta", "[ir][snapshot]") {
    NamespaceId ns(0);

    SECTION("compute entity added") {
        Snapshot before(SnapshotId(0), ns);
        Snapshot after(SnapshotId(1), ns);

        EntityRef entity(ns, 1);
        EntitySnapshot entity_snap;
        entity_snap.entity = entity;
        entity_snap.name = "New Entity";
        after.add_entity(std::move(entity_snap));

        auto delta = SnapshotDelta::compute(before, after);

        REQUIRE(delta.entity_changes().size() == 1);
        REQUIRE(delta.entity_changes()[0].type == SnapshotDelta::EntityChange::Type::Added);
        REQUIRE(delta.entity_changes()[0].entity == entity);
    }

    SECTION("compute entity removed") {
        Snapshot before(SnapshotId(0), ns);
        Snapshot after(SnapshotId(1), ns);

        EntityRef entity(ns, 1);
        EntitySnapshot entity_snap;
        entity_snap.entity = entity;
        before.add_entity(std::move(entity_snap));

        auto delta = SnapshotDelta::compute(before, after);

        REQUIRE(delta.entity_changes().size() == 1);
        REQUIRE(delta.entity_changes()[0].type == SnapshotDelta::EntityChange::Type::Removed);
    }

    SECTION("compute component added") {
        Snapshot before(SnapshotId(0), ns);
        Snapshot after(SnapshotId(1), ns);

        EntityRef entity(ns, 1);

        EntitySnapshot before_snap;
        before_snap.entity = entity;
        before.add_entity(std::move(before_snap));

        EntitySnapshot after_snap;
        after_snap.entity = entity;
        after_snap.components["Health"] = Value(100);
        after.add_entity(std::move(after_snap));

        auto delta = SnapshotDelta::compute(before, after);

        REQUIRE(delta.component_changes().size() == 1);
        REQUIRE(delta.component_changes()[0].type == SnapshotDelta::ComponentChange::Type::Added);
        REQUIRE(delta.component_changes()[0].component_type == "Health");
    }

    SECTION("compute component modified") {
        Snapshot before(SnapshotId(0), ns);
        Snapshot after(SnapshotId(1), ns);

        EntityRef entity(ns, 1);

        EntitySnapshot before_snap;
        before_snap.entity = entity;
        before_snap.components["Health"] = Value(100);
        before.add_entity(std::move(before_snap));

        EntitySnapshot after_snap;
        after_snap.entity = entity;
        after_snap.components["Health"] = Value(50);
        after.add_entity(std::move(after_snap));

        auto delta = SnapshotDelta::compute(before, after);

        REQUIRE(delta.component_changes().size() == 1);
        REQUIRE(delta.component_changes()[0].type == SnapshotDelta::ComponentChange::Type::Modified);
        REQUIRE(delta.component_changes()[0].old_value->as_int() == 100);
        REQUIRE(delta.component_changes()[0].new_value->as_int() == 50);
    }

    SECTION("compute component removed") {
        Snapshot before(SnapshotId(0), ns);
        Snapshot after(SnapshotId(1), ns);

        EntityRef entity(ns, 1);

        EntitySnapshot before_snap;
        before_snap.entity = entity;
        before_snap.components["Health"] = Value(100);
        before.add_entity(std::move(before_snap));

        EntitySnapshot after_snap;
        after_snap.entity = entity;
        // No Health component
        after.add_entity(std::move(after_snap));

        auto delta = SnapshotDelta::compute(before, after);

        REQUIRE(delta.component_changes().size() == 1);
        REQUIRE(delta.component_changes()[0].type == SnapshotDelta::ComponentChange::Type::Removed);
    }

    SECTION("to_patches") {
        Snapshot before(SnapshotId(0), ns);
        Snapshot after(SnapshotId(1), ns);

        EntityRef entity(ns, 1);
        EntitySnapshot after_snap;
        after_snap.entity = entity;
        after_snap.name = "Test";
        after_snap.components["Health"] = Value(100);
        after.add_entity(std::move(after_snap));

        auto delta = SnapshotDelta::compute(before, after);
        auto patches = delta.to_patches();

        // Should have entity create + component add
        REQUIRE(patches.size() >= 2);
    }

    SECTION("empty delta") {
        Snapshot snap1(SnapshotId(0), ns);
        Snapshot snap2(SnapshotId(1), ns);

        auto delta = SnapshotDelta::compute(snap1, snap2);
        REQUIRE(delta.empty());
    }
}

// =============================================================================
// SnapshotManager Tests
// =============================================================================

TEST_CASE("SnapshotManager", "[ir][snapshot]") {
    NamespaceId ns(0);

    SECTION("create snapshot") {
        SnapshotManager manager;
        SnapshotId id = manager.create(ns, "Test snapshot");

        REQUIRE(id.is_valid());
        REQUIRE(manager.size() == 1);
        REQUIRE(manager.get(id)->description() == "Test snapshot");
    }

    SECTION("latest snapshot") {
        SnapshotManager manager;
        manager.create(ns, "First");
        manager.create(ns, "Second");
        manager.create(ns, "Third");

        REQUIRE(manager.latest()->description() == "Third");
    }

    SECTION("at index") {
        SnapshotManager manager;
        manager.create(ns, "A");
        manager.create(ns, "B");
        manager.create(ns, "C");

        REQUIRE(manager.at_index(0)->description() == "A");
        REQUIRE(manager.at_index(1)->description() == "B");
        REQUIRE(manager.at_index(2)->description() == "C");
        REQUIRE(manager.at_index(3) == nullptr);
    }

    SECTION("remove snapshot") {
        SnapshotManager manager;
        SnapshotId id1 = manager.create(ns, "A");
        SnapshotId id2 = manager.create(ns, "B");

        REQUIRE(manager.remove(id1));
        REQUIRE(manager.size() == 1);
        REQUIRE(manager.get(id1) == nullptr);
        REQUIRE(manager.get(id2) != nullptr);
    }

    SECTION("remove before") {
        SnapshotManager manager;
        SnapshotId id1 = manager.create(ns, "A");
        SnapshotId id2 = manager.create(ns, "B");
        SnapshotId id3 = manager.create(ns, "C");

        std::size_t removed = manager.remove_before(id3);

        REQUIRE(removed == 2);
        REQUIRE(manager.size() == 1);
        REQUIRE(manager.get(id1) == nullptr);
        REQUIRE(manager.get(id2) == nullptr);
        REQUIRE(manager.get(id3) != nullptr);
    }

    SECTION("max snapshots limit") {
        SnapshotManager manager(3);  // Max 3 snapshots

        manager.create(ns, "A");
        manager.create(ns, "B");
        manager.create(ns, "C");
        REQUIRE(manager.size() == 3);

        manager.create(ns, "D");  // Should evict oldest
        REQUIRE(manager.size() == 3);
        REQUIRE(manager.at_index(0)->description() == "B");
    }

    SECTION("clear") {
        SnapshotManager manager;
        manager.create(ns, "A");
        manager.create(ns, "B");

        manager.clear();

        REQUIRE(manager.empty());
    }

    SECTION("snapshot ids in order") {
        SnapshotManager manager;
        SnapshotId id1 = manager.create(ns, "A");
        SnapshotId id2 = manager.create(ns, "B");
        SnapshotId id3 = manager.create(ns, "C");

        const auto& ids = manager.snapshot_ids();
        REQUIRE(ids.size() == 3);
        REQUIRE(ids[0] == id1);
        REQUIRE(ids[1] == id2);
        REQUIRE(ids[2] == id3);
    }
}
