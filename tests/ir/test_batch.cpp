// void_ir Batch optimization tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <void_engine/ir/ir.hpp>

using namespace void_ir;

// =============================================================================
// OptimizationStats Tests
// =============================================================================

TEST_CASE("OptimizationStats", "[ir][batch]") {
    SECTION("reduction percent") {
        OptimizationStats stats;
        stats.original_count = 100;
        stats.optimized_count = 75;

        REQUIRE(stats.reduction_percent() == Catch::Approx(25.0));
    }

    SECTION("zero original count") {
        OptimizationStats stats;
        stats.original_count = 0;
        stats.optimized_count = 0;

        REQUIRE(stats.reduction_percent() == Catch::Approx(0.0));
    }

    SECTION("combine stats") {
        OptimizationStats a;
        a.original_count = 50;
        a.optimized_count = 40;
        a.merged_count = 5;

        OptimizationStats b;
        b.original_count = 30;
        b.optimized_count = 25;
        b.merged_count = 3;

        a += b;

        REQUIRE(a.original_count == 80);
        REQUIRE(a.optimized_count == 65);
        REQUIRE(a.merged_count == 8);
    }
}

// =============================================================================
// BatchOptimizer Tests
// =============================================================================

TEST_CASE("BatchOptimizer empty batch", "[ir][batch]") {
    BatchOptimizer optimizer;
    PatchBatch empty;

    auto result = optimizer.optimize(empty);

    REQUIRE(result.empty());
    REQUIRE(optimizer.stats().original_count == 0);
}

TEST_CASE("BatchOptimizer eliminate contradictions", "[ir][batch]") {
    BatchOptimizer optimizer;
    NamespaceId ns(0);

    SECTION("create then delete") {
        EntityRef entity(ns, 1);

        PatchBatch batch;
        batch.push(EntityPatch::create(entity, "Test"));
        batch.push(ComponentPatch::add(entity, "Health", Value(100)));
        batch.push(EntityPatch::destroy(entity));

        auto result = optimizer.optimize(batch);

        // All patches for this entity should be eliminated
        REQUIRE(result.empty());
        REQUIRE(optimizer.stats().eliminated_count > 0);
    }

    SECTION("enable then disable") {
        EntityRef entity(ns, 1);

        PatchBatch batch;
        batch.push(EntityPatch::enable(entity));
        batch.push(EntityPatch::disable(entity));

        auto result = optimizer.optimize(batch);

        // Enable/disable pair eliminated
        REQUIRE(result.empty());
    }

    SECTION("keeps independent entities") {
        EntityRef entity1(ns, 1);
        EntityRef entity2(ns, 2);

        PatchBatch batch;
        batch.push(EntityPatch::create(entity1, "A"));
        batch.push(EntityPatch::destroy(entity1));  // Contradicts entity1
        batch.push(EntityPatch::create(entity2, "B"));  // Independent

        auto result = optimizer.optimize(batch);

        // entity2 should remain
        REQUIRE(result.size() == 1);
    }
}

TEST_CASE("BatchOptimizer merge consecutive", "[ir][batch]") {
    BatchOptimizer optimizer;
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("merge Set operations") {
        PatchBatch batch;
        batch.push(ComponentPatch::set(entity, "Health", Value(100)));
        batch.push(ComponentPatch::set(entity, "Health", Value(75)));
        batch.push(ComponentPatch::set(entity, "Health", Value(50)));

        auto result = optimizer.optimize(batch);

        // Should be merged to single Set with final value
        REQUIRE(result.size() == 1);
        REQUIRE(result.patches()[0].as<ComponentPatch>().value.as_int() == 50);
    }

    SECTION("merge SetField into Set") {
        Value health = Value::empty_object();
        health["current"] = Value(100);
        health["max"] = Value(100);

        PatchBatch batch;
        batch.push(ComponentPatch::set(entity, "Health", std::move(health)));
        batch.push(ComponentPatch::set_field(entity, "Health", "current", Value(75)));

        auto result = optimizer.optimize(batch);

        // Should be merged
        REQUIRE(result.size() == 1);

        const auto& merged = result.patches()[0].as<ComponentPatch>();
        REQUIRE(merged.value["current"].as_int() == 75);
        REQUIRE(merged.value["max"].as_int() == 100);
    }

    SECTION("different components not merged") {
        PatchBatch batch;
        batch.push(ComponentPatch::set(entity, "Health", Value(100)));
        batch.push(ComponentPatch::set(entity, "Armor", Value(50)));

        auto result = optimizer.optimize(batch);

        // Different components, no merge
        REQUIRE(result.size() == 2);
    }

    SECTION("different entities not merged") {
        EntityRef entity2(ns, 2);

        PatchBatch batch;
        batch.push(ComponentPatch::set(entity, "Health", Value(100)));
        batch.push(ComponentPatch::set(entity2, "Health", Value(100)));

        auto result = optimizer.optimize(batch);

        // Different entities, no merge
        REQUIRE(result.size() == 2);
    }
}

TEST_CASE("BatchOptimizer coalesce field patches", "[ir][batch]") {
    BatchOptimizer optimizer;
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("coalesce multiple SetField to Set") {
        PatchBatch batch;
        batch.push(ComponentPatch::set_field(entity, "Transform", "x", Value(1.0)));
        batch.push(ComponentPatch::set_field(entity, "Transform", "y", Value(2.0)));
        batch.push(ComponentPatch::set_field(entity, "Transform", "z", Value(3.0)));

        auto result = optimizer.optimize(batch);

        // 3 field patches coalesced into 1 Set
        REQUIRE(result.size() == 1);

        const auto& patch = result.patches()[0].as<ComponentPatch>();
        REQUIRE(patch.operation == ComponentOp::Set);
        REQUIRE(patch.value.is_object());
    }

    SECTION("few field patches not coalesced") {
        PatchBatch batch;
        batch.push(ComponentPatch::set_field(entity, "Transform", "x", Value(1.0)));
        batch.push(ComponentPatch::set_field(entity, "Transform", "y", Value(2.0)));

        auto result = optimizer.optimize(batch);

        // Only 2 patches, not worth coalescing
        REQUIRE(result.size() == 2);
    }
}

TEST_CASE("BatchOptimizer sorting", "[ir][batch]") {
    BatchOptimizer optimizer;
    NamespaceId ns(0);

    SECTION("entity patches first") {
        EntityRef entity(ns, 1);

        PatchBatch batch;
        batch.push(ComponentPatch::add(entity, "Health", Value(100)));
        batch.push(EntityPatch::create(entity, "Test"));

        auto result = optimizer.optimize(batch);

        // Entity create should come before component add
        REQUIRE(result.patches()[0].kind() == PatchKind::Entity);
        REQUIRE(result.patches()[1].kind() == PatchKind::Component);
    }

    SECTION("creates before other entity ops") {
        EntityRef entity1(ns, 1);
        EntityRef entity2(ns, 2);

        PatchBatch batch;
        batch.push(EntityPatch::enable(entity1));
        batch.push(EntityPatch::create(entity2, "New"));

        auto result = optimizer.optimize(batch);

        // Create should come before enable
        REQUIRE(result.patches()[0].as<EntityPatch>().operation == EntityOp::Create);
    }

    SECTION("group by entity id") {
        EntityRef entity1(ns, 1);
        EntityRef entity2(ns, 2);

        PatchBatch batch;
        batch.push(ComponentPatch::set(entity2, "A", Value(1)));
        batch.push(ComponentPatch::set(entity1, "B", Value(2)));
        batch.push(ComponentPatch::set(entity2, "C", Value(3)));
        batch.push(ComponentPatch::set(entity1, "D", Value(4)));

        auto result = optimizer.optimize(batch);

        // Should be grouped by entity ID
        auto e1 = result.patches()[0].target_entity()->entity_id;
        auto e2 = result.patches()[1].target_entity()->entity_id;
        auto e3 = result.patches()[2].target_entity()->entity_id;
        auto e4 = result.patches()[3].target_entity()->entity_id;

        // entity1 patches together, entity2 patches together
        REQUIRE(((e1 == e2 && e3 == e4) || (e1 == 1 && e2 == 1)));
    }
}

TEST_CASE("BatchOptimizer options", "[ir][batch]") {
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("no optimizations") {
        BatchOptimizer optimizer(BatchOptimizer::Options::none());

        PatchBatch batch;
        batch.push(EntityPatch::create(entity, "Test"));
        batch.push(EntityPatch::destroy(entity));

        auto result = optimizer.optimize(batch);

        // Should keep both (no contradiction elimination)
        REQUIRE(result.size() == 2);
    }

    SECTION("selective optimizations") {
        BatchOptimizer::Options opts;
        opts.eliminate_contradictions = true;
        opts.merge_consecutive = false;

        BatchOptimizer optimizer(opts);

        PatchBatch batch;
        batch.push(ComponentPatch::set(entity, "Health", Value(100)));
        batch.push(ComponentPatch::set(entity, "Health", Value(50)));

        auto result = optimizer.optimize(batch);

        // No merge, both patches kept
        REQUIRE(result.size() == 2);
    }
}

// =============================================================================
// PatchDeduplicator Tests
// =============================================================================

TEST_CASE("PatchDeduplicator", "[ir][batch]") {
    PatchDeduplicator dedup;
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("remove duplicates keeping last") {
        PatchBatch batch;
        batch.push(ComponentPatch::set(entity, "Health", Value(100)));
        batch.push(ComponentPatch::set(entity, "Health", Value(75)));  // Duplicate key
        batch.push(ComponentPatch::set(entity, "Health", Value(50)));  // Duplicate key

        auto result = dedup.deduplicate(batch);

        // Keeps last occurrence
        REQUIRE(result.size() == 1);
        REQUIRE(dedup.removed_count() == 2);
    }

    SECTION("different targets not deduplicated") {
        EntityRef entity2(ns, 2);

        PatchBatch batch;
        batch.push(ComponentPatch::set(entity, "Health", Value(100)));
        batch.push(ComponentPatch::set(entity2, "Health", Value(100)));

        auto result = dedup.deduplicate(batch);

        REQUIRE(result.size() == 2);
        REQUIRE(dedup.removed_count() == 0);
    }

    SECTION("different fields not deduplicated") {
        PatchBatch batch;
        batch.push(ComponentPatch::set_field(entity, "Transform", "x", Value(1.0)));
        batch.push(ComponentPatch::set_field(entity, "Transform", "y", Value(2.0)));

        auto result = dedup.deduplicate(batch);

        REQUIRE(result.size() == 2);
    }
}

// =============================================================================
// PatchSplitter Tests
// =============================================================================

TEST_CASE("PatchSplitter", "[ir][batch]") {
    PatchSplitter splitter;

    SECTION("split by namespace") {
        NamespaceId ns1(0);
        NamespaceId ns2(1);

        PatchBatch batch;
        batch.push(EntityPatch::create(EntityRef(ns1, 1), "A"));
        batch.push(EntityPatch::create(EntityRef(ns1, 2), "B"));
        batch.push(EntityPatch::create(EntityRef(ns2, 1), "C"));

        auto split = splitter.split_by_namespace(batch);

        REQUIRE(split.size() == 2);
        REQUIRE(split[0].size() == 2);
        REQUIRE(split[1].size() == 1);
    }

    SECTION("split by entity") {
        NamespaceId ns(0);
        EntityRef entity1(ns, 1);
        EntityRef entity2(ns, 2);

        PatchBatch batch;
        batch.push(ComponentPatch::set(entity1, "A", Value(1)));
        batch.push(ComponentPatch::set(entity1, "B", Value(2)));
        batch.push(ComponentPatch::set(entity2, "A", Value(3)));

        auto split = splitter.split_by_entity(batch);

        REQUIRE(split.size() == 2);
        REQUIRE(split[1].size() == 2);
        REQUIRE(split[2].size() == 1);
    }

    SECTION("split by kind") {
        NamespaceId ns(0);
        EntityRef entity(ns, 1);

        PatchBatch batch;
        batch.push(EntityPatch::create(entity, "Test"));
        batch.push(ComponentPatch::add(entity, "Health", Value(100)));
        batch.push(TransformPatch::set_position(entity, Vec3{0, 0, 0}));
        batch.push(ComponentPatch::add(entity, "Armor", Value(50)));

        auto split = splitter.split_by_kind(batch);

        REQUIRE(split[PatchKind::Entity].size() == 1);
        REQUIRE(split[PatchKind::Component].size() == 2);
        REQUIRE(split[PatchKind::Transform].size() == 1);
    }
}
