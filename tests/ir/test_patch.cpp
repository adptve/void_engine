// void_ir Patch tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/ir/ir.hpp>

using namespace void_ir;

// =============================================================================
// EntityPatch Tests
// =============================================================================

TEST_CASE("EntityPatch creation", "[ir][patch]") {
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("create") {
        auto patch = EntityPatch::create(entity, "Player");
        REQUIRE(patch.entity == entity);
        REQUIRE(patch.operation == EntityOp::Create);
        REQUIRE(patch.name == "Player");
    }

    SECTION("destroy") {
        auto patch = EntityPatch::destroy(entity);
        REQUIRE(patch.operation == EntityOp::Delete);
    }

    SECTION("enable/disable") {
        auto enable = EntityPatch::enable(entity);
        auto disable = EntityPatch::disable(entity);

        REQUIRE(enable.operation == EntityOp::Enable);
        REQUIRE(disable.operation == EntityOp::Disable);
    }
}

// =============================================================================
// ComponentPatch Tests
// =============================================================================

TEST_CASE("ComponentPatch creation", "[ir][patch]") {
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("add component") {
        Value health = Value::empty_object();
        health["current"] = Value(100);
        health["max"] = Value(100);

        auto patch = ComponentPatch::add(entity, "Health", std::move(health));

        REQUIRE(patch.entity == entity);
        REQUIRE(patch.component_type == "Health");
        REQUIRE(patch.operation == ComponentOp::Add);
        REQUIRE(patch.value.is_object());
    }

    SECTION("remove component") {
        auto patch = ComponentPatch::remove(entity, "Health");

        REQUIRE(patch.operation == ComponentOp::Remove);
        REQUIRE(patch.value.is_null());
    }

    SECTION("set component") {
        auto patch = ComponentPatch::set(entity, "Health", Value(50));

        REQUIRE(patch.operation == ComponentOp::Set);
    }

    SECTION("set field") {
        auto patch = ComponentPatch::set_field(entity, "Health", "current", Value(75));

        REQUIRE(patch.operation == ComponentOp::SetField);
        REQUIRE(patch.field_path == "current");
        REQUIRE(patch.value.as_int() == 75);
    }
}

// =============================================================================
// TransformPatch Tests
// =============================================================================

TEST_CASE("TransformPatch creation", "[ir][patch]") {
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("position") {
        auto patch = TransformPatch::set_position(entity, Vec3{1.0f, 2.0f, 3.0f});

        REQUIRE(patch.property == TransformProperty::Position);
        REQUIRE(patch.value.is_vec3());
        REQUIRE(patch.value.as_vec3().x == 1.0f);
    }

    SECTION("rotation") {
        auto patch = TransformPatch::set_rotation(entity, Vec4{0, 0, 0, 1});

        REQUIRE(patch.property == TransformProperty::Rotation);
        REQUIRE(patch.value.is_vec4());
    }

    SECTION("scale") {
        auto patch = TransformPatch::set_scale(entity, Vec3{2.0f, 2.0f, 2.0f});

        REQUIRE(patch.property == TransformProperty::Scale);
    }

    SECTION("local variants") {
        auto local_pos = TransformPatch::set_local_position(entity, Vec3{0, 0, 0});
        auto local_rot = TransformPatch::set_local_rotation(entity, Vec4{0, 0, 0, 1});
        auto local_scale = TransformPatch::set_local_scale(entity, Vec3{1, 1, 1});

        REQUIRE(local_pos.property == TransformProperty::LocalPosition);
        REQUIRE(local_rot.property == TransformProperty::LocalRotation);
        REQUIRE(local_scale.property == TransformProperty::LocalScale);
    }
}

// =============================================================================
// HierarchyPatch Tests
// =============================================================================

TEST_CASE("HierarchyPatch creation", "[ir][patch]") {
    NamespaceId ns(0);
    EntityRef child(ns, 1);
    EntityRef parent(ns, 2);

    SECTION("set parent") {
        auto patch = HierarchyPatch::set_parent(child, parent);

        REQUIRE(patch.entity == child);
        REQUIRE(patch.operation == HierarchyOp::SetParent);
        REQUIRE(patch.parent == parent);
    }

    SECTION("clear parent") {
        auto patch = HierarchyPatch::clear_parent(child);

        REQUIRE(patch.operation == HierarchyOp::ClearParent);
    }

    SECTION("sibling index") {
        auto patch = HierarchyPatch::set_sibling_index(child, 5);

        REQUIRE(patch.operation == HierarchyOp::SetSiblingIndex);
        REQUIRE(patch.sibling_index == 5);
    }
}

// =============================================================================
// Patch Variant Tests
// =============================================================================

TEST_CASE("Patch variant wrapper", "[ir][patch]") {
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("kind detection") {
        Patch entity_patch(EntityPatch::create(entity, "Test"));
        Patch component_patch(ComponentPatch::add(entity, "Health", Value(100)));
        Patch transform_patch(TransformPatch::set_position(entity, Vec3{0, 0, 0}));

        REQUIRE(entity_patch.kind() == PatchKind::Entity);
        REQUIRE(component_patch.kind() == PatchKind::Component);
        REQUIRE(transform_patch.kind() == PatchKind::Transform);
    }

    SECTION("type checking") {
        Patch patch(EntityPatch::create(entity, "Test"));

        REQUIRE(patch.is<EntityPatch>());
        REQUIRE_FALSE(patch.is<ComponentPatch>());
    }

    SECTION("accessor") {
        Patch patch(ComponentPatch::set(entity, "Health", Value(50)));

        const auto& cp = patch.as<ComponentPatch>();
        REQUIRE(cp.component_type == "Health");

        REQUIRE(patch.try_as<ComponentPatch>() != nullptr);
        REQUIRE(patch.try_as<EntityPatch>() == nullptr);
    }

    SECTION("target entity") {
        Patch entity_patch(EntityPatch::create(entity, "Test"));
        Patch component_patch(ComponentPatch::set(entity, "Health", Value(100)));

        auto e1 = entity_patch.target_entity();
        auto e2 = component_patch.target_entity();

        REQUIRE(e1.has_value());
        REQUIRE(*e1 == entity);
        REQUIRE(e2.has_value());
        REQUIRE(*e2 == entity);
    }

    SECTION("visit") {
        Patch patch(ComponentPatch::set(entity, "Health", Value(100)));

        std::string visited_type;
        patch.visit([&](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, ComponentPatch>) {
                visited_type = "ComponentPatch";
            } else {
                visited_type = "Other";
            }
        });

        REQUIRE(visited_type == "ComponentPatch");
    }
}

// =============================================================================
// PatchBatch Tests
// =============================================================================

TEST_CASE("PatchBatch operations", "[ir][patch]") {
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("push and size") {
        PatchBatch batch;
        REQUIRE(batch.empty());

        batch.push(EntityPatch::create(entity, "Test"));
        batch.push(ComponentPatch::add(entity, "Health", Value(100)));

        REQUIRE(batch.size() == 2);
        REQUIRE_FALSE(batch.empty());
    }

    SECTION("iteration") {
        PatchBatch batch;
        batch.push(EntityPatch::create(entity, "A"));
        batch.push(EntityPatch::create(EntityRef(ns, 2), "B"));

        std::size_t count = 0;
        for (const auto& patch : batch) {
            REQUIRE(patch.kind() == PatchKind::Entity);
            count++;
        }
        REQUIRE(count == 2);
    }

    SECTION("append") {
        PatchBatch batch1;
        batch1.push(EntityPatch::create(entity, "A"));

        PatchBatch batch2;
        batch2.push(EntityPatch::create(EntityRef(ns, 2), "B"));

        batch1.append(batch2);
        REQUIRE(batch1.size() == 2);
    }

    SECTION("clear") {
        PatchBatch batch;
        batch.push(EntityPatch::create(entity, "Test"));
        batch.clear();

        REQUIRE(batch.empty());
    }
}
