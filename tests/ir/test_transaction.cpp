// void_ir Transaction tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/ir/ir.hpp>

using namespace void_ir;

// =============================================================================
// Namespace Tests
// =============================================================================

TEST_CASE("NamespaceId", "[ir][namespace]") {
    SECTION("default is invalid") {
        NamespaceId id;
        REQUIRE_FALSE(id.is_valid());
    }

    SECTION("explicit construction") {
        NamespaceId id(5);
        REQUIRE(id.is_valid());
        REQUIRE(id.value == 5);
    }

    SECTION("comparison") {
        NamespaceId a(1);
        NamespaceId b(1);
        NamespaceId c(2);

        REQUIRE(a == b);
        REQUIRE(a != c);
        REQUIRE(a < c);
    }
}

TEST_CASE("EntityRef", "[ir][namespace]") {
    NamespaceId ns(0);

    SECTION("construction") {
        EntityRef ref(ns, 42);
        REQUIRE(ref.namespace_id == ns);
        REQUIRE(ref.entity_id == 42);
        REQUIRE(ref.is_valid());
    }

    SECTION("comparison") {
        EntityRef a(ns, 1);
        EntityRef b(ns, 1);
        EntityRef c(ns, 2);

        REQUIRE(a == b);
        REQUIRE(a != c);
        REQUIRE(a < c);
    }
}

TEST_CASE("NamespacePermissions", "[ir][namespace]") {
    SECTION("full permissions") {
        auto perms = NamespacePermissions::full();
        REQUIRE(perms.can_create_entities);
        REQUIRE(perms.can_delete_entities);
        REQUIRE(perms.can_modify_components);
    }

    SECTION("read-only permissions") {
        auto perms = NamespacePermissions::read_only();
        REQUIRE_FALSE(perms.can_create_entities);
        REQUIRE_FALSE(perms.can_delete_entities);
        REQUIRE_FALSE(perms.can_modify_components);
    }

    SECTION("component filtering") {
        NamespacePermissions perms;
        perms.allowed_components = {"Transform", "Mesh"};
        perms.blocked_components = {"Debug"};

        REQUIRE(perms.is_component_allowed("Transform"));
        REQUIRE(perms.is_component_allowed("Mesh"));
        REQUIRE_FALSE(perms.is_component_allowed("Health"));
        REQUIRE_FALSE(perms.is_component_allowed("Debug"));
    }
}

TEST_CASE("ResourceLimits", "[ir][namespace]") {
    SECTION("unlimited") {
        auto limits = ResourceLimits::unlimited();
        REQUIRE(limits.max_entities == 0);
        REQUIRE(limits.max_memory_bytes == 0);
    }

    SECTION("sandboxed") {
        auto limits = ResourceLimits::sandboxed();
        REQUIRE(limits.max_entities == 10000);
        REQUIRE(limits.max_memory_bytes == 64 * 1024 * 1024);
    }

    SECTION("usage checking") {
        ResourceLimits limits;
        limits.max_entities = 100;

        ResourceUsage usage;
        usage.entity_count = 50;
        REQUIRE(usage.within_limits(limits));

        usage.entity_count = 100;
        REQUIRE_FALSE(usage.within_limits(limits));
    }
}

TEST_CASE("Namespace", "[ir][namespace]") {
    SECTION("construction") {
        Namespace ns(NamespaceId(0), "game");
        REQUIRE(ns.id() == NamespaceId(0));
        REQUIRE(ns.name() == "game");
    }

    SECTION("entity allocation") {
        Namespace ns(NamespaceId(0), "game");
        REQUIRE(ns.peek_next_entity_id() == 1);

        std::uint64_t e1 = ns.allocate_entity();
        std::uint64_t e2 = ns.allocate_entity();

        REQUIRE(e1 == 1);
        REQUIRE(e2 == 2);
    }

    SECTION("permissions") {
        Namespace ns(NamespaceId(0), "game");
        ns.set_permissions(NamespacePermissions::read_only());

        REQUIRE_FALSE(ns.permissions().can_create_entities);
    }
}

TEST_CASE("NamespaceRegistry", "[ir][namespace]") {
    NamespaceRegistry registry;

    SECTION("create namespace") {
        NamespaceId id = registry.create("game");
        REQUIRE(id.is_valid());
        REQUIRE(registry.size() == 1);
    }

    SECTION("get by id") {
        NamespaceId id = registry.create("game");
        Namespace* ns = registry.get(id);

        REQUIRE(ns != nullptr);
        REQUIRE(ns->name() == "game");
    }

    SECTION("find by name") {
        NamespaceId id = registry.create("game");
        auto found = registry.find_by_name("game");

        REQUIRE(found.has_value());
        REQUIRE(*found == id);

        REQUIRE_FALSE(registry.find_by_name("unknown").has_value());
    }

    SECTION("multiple namespaces") {
        NamespaceId game = registry.create("game");
        NamespaceId ui = registry.create("ui");
        NamespaceId debug = registry.create("debug");

        REQUIRE(registry.size() == 3);
        REQUIRE(registry.get(game)->name() == "game");
        REQUIRE(registry.get(ui)->name() == "ui");
        REQUIRE(registry.get(debug)->name() == "debug");
    }
}

// =============================================================================
// Transaction Tests
// =============================================================================

TEST_CASE("TransactionId", "[ir][transaction]") {
    SECTION("default is invalid") {
        TransactionId id;
        REQUIRE_FALSE(id.is_valid());
    }

    SECTION("explicit construction") {
        TransactionId id(42);
        REQUIRE(id.is_valid());
        REQUIRE(id.value == 42);
    }
}

TEST_CASE("Transaction lifecycle", "[ir][transaction]") {
    NamespaceId ns(0);
    TransactionId tx_id(0);

    SECTION("initial state is Building") {
        Transaction tx(tx_id, ns);
        REQUIRE(tx.state() == TransactionState::Building);
    }

    SECTION("add patches in Building state") {
        Transaction tx(tx_id, ns);
        EntityRef entity(ns, 1);

        tx.add_patch(EntityPatch::create(entity, "Test"));
        REQUIRE(tx.patch_count() == 1);
    }

    SECTION("submit moves to Pending") {
        Transaction tx(tx_id, ns);
        tx.submit();
        REQUIRE(tx.state() == TransactionState::Pending);
    }

    SECTION("cannot add patches after submit") {
        Transaction tx(tx_id, ns);
        tx.submit();

        EntityRef entity(ns, 1);
        REQUIRE_THROWS(tx.add_patch(EntityPatch::create(entity, "Test")));
    }

    SECTION("state transitions") {
        Transaction tx(tx_id, ns);

        tx.submit();
        REQUIRE(tx.state() == TransactionState::Pending);

        tx.begin_apply();
        REQUIRE(tx.state() == TransactionState::Applying);

        tx.commit();
        REQUIRE(tx.state() == TransactionState::Committed);
    }

    SECTION("rollback") {
        Transaction tx(tx_id, ns);
        tx.submit();
        tx.begin_apply();
        tx.rollback();

        REQUIRE(tx.state() == TransactionState::RolledBack);
    }

    SECTION("fail with error") {
        Transaction tx(tx_id, ns);
        tx.submit();
        tx.begin_apply();
        tx.fail("Test error");

        REQUIRE(tx.state() == TransactionState::Failed);
        REQUIRE(tx.error() == "Test error");
    }
}

TEST_CASE("TransactionBuilder", "[ir][transaction]") {
    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("build simple transaction") {
        TransactionBuilder builder(ns);
        builder
            .description("Create player")
            .create_entity(entity, "Player")
            .add_component(entity, "Health", Value(100));

        Transaction tx = builder.build(TransactionId(0));

        REQUIRE(tx.patch_count() == 2);
        REQUIRE(tx.metadata().description == "Create player");
    }

    SECTION("set position and rotation") {
        TransactionBuilder builder(ns);
        builder
            .set_position(entity, Vec3{1, 2, 3})
            .set_rotation(entity, Vec4{0, 0, 0, 1})
            .set_scale(entity, Vec3{1, 1, 1});

        Transaction tx = builder.build(TransactionId(0));
        REQUIRE(tx.patch_count() == 3);
    }

    SECTION("hierarchy operations") {
        EntityRef child(ns, 2);

        TransactionBuilder builder(ns);
        builder
            .set_parent(child, entity)
            .clear_parent(child);

        Transaction tx = builder.build(TransactionId(0));
        REQUIRE(tx.patch_count() == 2);
    }

    SECTION("priority") {
        TransactionBuilder builder(ns);
        builder.priority(TransactionPriority::High);

        Transaction tx = builder.build(TransactionId(0));
        REQUIRE(tx.metadata().priority == TransactionPriority::High);
    }
}

TEST_CASE("TransactionQueue", "[ir][transaction]") {
    TransactionQueue queue;
    NamespaceId ns(0);

    SECTION("empty queue") {
        REQUIRE(queue.empty());
        REQUIRE(queue.size() == 0);
        REQUIRE_FALSE(queue.dequeue().has_value());
    }

    SECTION("enqueue and dequeue") {
        Transaction tx(TransactionId(0), ns);
        queue.enqueue(std::move(tx));

        REQUIRE(queue.size() == 1);

        auto dequeued = queue.dequeue();
        REQUIRE(dequeued.has_value());
        REQUIRE(dequeued->id() == TransactionId(0));
        REQUIRE(queue.empty());
    }

    SECTION("priority ordering") {
        TransactionBuilder low_builder(ns);
        low_builder.priority(TransactionPriority::Low);
        Transaction low_tx = low_builder.build(TransactionId(0));

        TransactionBuilder high_builder(ns);
        high_builder.priority(TransactionPriority::High);
        Transaction high_tx = high_builder.build(TransactionId(1));

        queue.enqueue(std::move(low_tx));
        queue.enqueue(std::move(high_tx));

        // High priority should come out first
        auto first = queue.dequeue();
        REQUIRE(first->metadata().priority == TransactionPriority::High);

        auto second = queue.dequeue();
        REQUIRE(second->metadata().priority == TransactionPriority::Low);
    }

    SECTION("peek") {
        Transaction tx(TransactionId(0), ns);
        queue.enqueue(std::move(tx));

        const Transaction* peeked = queue.peek();
        REQUIRE(peeked != nullptr);
        REQUIRE(queue.size() == 1);  // Still in queue
    }

    SECTION("total patch count") {
        EntityRef entity(ns, 1);

        TransactionBuilder builder1(ns);
        builder1.create_entity(entity, "A");
        queue.enqueue(builder1.build(TransactionId(0)));

        TransactionBuilder builder2(ns);
        builder2
            .create_entity(EntityRef(ns, 2), "B")
            .add_component(EntityRef(ns, 2), "Health", Value(100));
        queue.enqueue(builder2.build(TransactionId(1)));

        REQUIRE(queue.total_patch_count() == 3);
    }
}
