// void_ir Validation tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/ir/ir.hpp>

using namespace void_ir;

// =============================================================================
// FieldDescriptor Tests
// =============================================================================

TEST_CASE("FieldDescriptor creation", "[ir][validation]") {
    SECTION("boolean field") {
        auto field = FieldDescriptor::boolean("enabled");
        REQUIRE(field.name == "enabled");
        REQUIRE(field.type == FieldType::Bool);
        REQUIRE(field.required);
    }

    SECTION("integer field") {
        auto field = FieldDescriptor::integer("count");
        REQUIRE(field.type == FieldType::Int);
    }

    SECTION("integer with range") {
        auto field = FieldDescriptor::integer_range("health", 0, 100);
        REQUIRE(field.numeric_range.has_value());
        REQUIRE(field.numeric_range->min == 0);
        REQUIRE(field.numeric_range->max == 100);
    }

    SECTION("float field") {
        auto field = FieldDescriptor::floating("speed");
        REQUIRE(field.type == FieldType::Float);
    }

    SECTION("float with range") {
        auto field = FieldDescriptor::float_range("opacity", 0.0, 1.0);
        REQUIRE(field.numeric_range.has_value());
    }

    SECTION("string field") {
        auto field = FieldDescriptor::string("name");
        REQUIRE(field.type == FieldType::String);
    }

    SECTION("vec3 field") {
        auto field = FieldDescriptor::vec3("position");
        REQUIRE(field.type == FieldType::Vec3);
    }

    SECTION("enum field") {
        auto field = FieldDescriptor::enumeration("state", {"idle", "running", "jumping"});
        REQUIRE(field.type == FieldType::Enum);
        REQUIRE(field.enum_values.size() == 3);
    }

    SECTION("with default") {
        auto field = FieldDescriptor::integer("count").with_default(Value(0));
        REQUIRE_FALSE(field.required);
        REQUIRE(field.default_value.as_int() == 0);
    }

    SECTION("nullable") {
        auto field = FieldDescriptor::string("description").make_nullable();
        REQUIRE(field.nullable);
    }
}

// =============================================================================
// ComponentSchema Tests
// =============================================================================

TEST_CASE("ComponentSchema", "[ir][validation]") {
    SECTION("create schema") {
        ComponentSchema schema("Transform");
        schema
            .field(FieldDescriptor::vec3("position"))
            .field(FieldDescriptor::vec4("rotation"))
            .field(FieldDescriptor::vec3("scale").with_default(Value(Vec3{1, 1, 1})));

        REQUIRE(schema.type_name() == "Transform");
        REQUIRE(schema.fields().size() == 3);
    }

    SECTION("find field") {
        ComponentSchema schema("Health");
        schema.field(FieldDescriptor::integer("current"));

        REQUIRE(schema.find_field("current") != nullptr);
        REQUIRE(schema.find_field("unknown") == nullptr);
    }

    SECTION("validate valid object") {
        ComponentSchema schema("Health");
        schema
            .field(FieldDescriptor::integer("current"))
            .field(FieldDescriptor::integer("max"));

        Value valid = Value::empty_object();
        valid["current"] = Value(100);
        valid["max"] = Value(100);

        auto result = schema.validate(valid);
        REQUIRE(result.valid);
    }

    SECTION("validate missing required field") {
        ComponentSchema schema("Health");
        schema
            .field(FieldDescriptor::integer("current"))
            .field(FieldDescriptor::integer("max"));

        Value invalid = Value::empty_object();
        invalid["current"] = Value(100);
        // missing "max"

        auto result = schema.validate(invalid);
        REQUIRE_FALSE(result.valid);
        REQUIRE(result.errors.size() >= 1);
    }

    SECTION("validate type mismatch") {
        ComponentSchema schema("Health");
        schema.field(FieldDescriptor::integer("current"));

        Value invalid = Value::empty_object();
        invalid["current"] = Value("not a number");

        auto result = schema.validate(invalid);
        REQUIRE_FALSE(result.valid);
    }

    SECTION("validate numeric range") {
        ComponentSchema schema("Health");
        schema.field(FieldDescriptor::integer_range("current", 0, 100));

        Value out_of_range = Value::empty_object();
        out_of_range["current"] = Value(150);

        auto result = schema.validate(out_of_range);
        REQUIRE_FALSE(result.valid);
    }

    SECTION("validate enum values") {
        ComponentSchema schema("State");
        schema.field(FieldDescriptor::enumeration("current", {"idle", "running"}));

        Value valid = Value::empty_object();
        valid["current"] = Value("idle");
        REQUIRE(schema.validate(valid).valid);

        Value invalid = Value::empty_object();
        invalid["current"] = Value("flying");
        REQUIRE_FALSE(schema.validate(invalid).valid);
    }

    SECTION("nullable field accepts null") {
        ComponentSchema schema("Config");
        schema.field(FieldDescriptor::string("description").make_nullable());

        Value with_null = Value::empty_object();
        with_null["description"] = Value::null();

        REQUIRE(schema.validate(with_null).valid);
    }

    SECTION("non-nullable rejects null") {
        ComponentSchema schema("Config");
        schema.field(FieldDescriptor::string("name"));  // required, non-nullable

        Value with_null = Value::empty_object();
        with_null["name"] = Value::null();

        REQUIRE_FALSE(schema.validate(with_null).valid);
    }
}

// =============================================================================
// SchemaRegistry Tests
// =============================================================================

TEST_CASE("SchemaRegistry", "[ir][validation]") {
    SchemaRegistry registry;

    SECTION("register and get") {
        ComponentSchema schema("Transform");
        schema.field(FieldDescriptor::vec3("position"));

        registry.register_schema(std::move(schema));

        const ComponentSchema* found = registry.get("Transform");
        REQUIRE(found != nullptr);
        REQUIRE(found->type_name() == "Transform");
    }

    SECTION("has schema") {
        ComponentSchema schema("Transform");
        registry.register_schema(std::move(schema));

        REQUIRE(registry.has("Transform"));
        REQUIRE_FALSE(registry.has("Unknown"));
    }

    SECTION("type names") {
        registry.register_schema(ComponentSchema("A"));
        registry.register_schema(ComponentSchema("B"));
        registry.register_schema(ComponentSchema("C"));

        auto names = registry.type_names();
        REQUIRE(names.size() == 3);
    }

    SECTION("validate patch with schema") {
        ComponentSchema schema("Health");
        schema
            .field(FieldDescriptor::integer("current"))
            .field(FieldDescriptor::integer("max"));
        registry.register_schema(std::move(schema));

        NamespaceId ns(0);
        EntityRef entity(ns, 1);

        // Valid patch
        Value valid = Value::empty_object();
        valid["current"] = Value(100);
        valid["max"] = Value(100);

        auto valid_patch = ComponentPatch::add(entity, "Health", std::move(valid));
        auto result = registry.validate_patch(valid_patch);
        REQUIRE(result.valid);

        // Invalid patch
        Value invalid = Value::empty_object();
        invalid["current"] = Value("not a number");
        invalid["max"] = Value(100);

        auto invalid_patch = ComponentPatch::add(entity, "Health", std::move(invalid));
        auto invalid_result = registry.validate_patch(invalid_patch);
        REQUIRE_FALSE(invalid_result.valid);
    }

    SECTION("no schema means no validation") {
        NamespaceId ns(0);
        EntityRef entity(ns, 1);

        auto patch = ComponentPatch::add(entity, "Unknown", Value(42));
        auto result = registry.validate_patch(patch);

        // No schema = passes validation
        REQUIRE(result.valid);
    }
}

// =============================================================================
// PatchValidator Tests
// =============================================================================

TEST_CASE("PatchValidator", "[ir][validation]") {
    SchemaRegistry schemas;
    PatchValidator validator(schemas);

    NamespaceId ns(0);
    EntityRef entity(ns, 1);

    SECTION("permission check - create entity") {
        NamespacePermissions perms;
        perms.can_create_entities = false;

        Patch patch(EntityPatch::create(entity, "Test"));
        auto result = validator.validate(patch, perms);

        REQUIRE_FALSE(result.valid);
    }

    SECTION("permission check - delete entity") {
        NamespacePermissions perms;
        perms.can_delete_entities = false;

        Patch patch(EntityPatch::destroy(entity));
        auto result = validator.validate(patch, perms);

        REQUIRE_FALSE(result.valid);
    }

    SECTION("permission check - modify components") {
        NamespacePermissions perms;
        perms.can_modify_components = false;

        Patch patch(ComponentPatch::set(entity, "Health", Value(100)));
        auto result = validator.validate(patch, perms);

        REQUIRE_FALSE(result.valid);
    }

    SECTION("component type not allowed") {
        NamespacePermissions perms;
        perms.allowed_components = {"Transform"};

        Patch patch(ComponentPatch::set(entity, "Health", Value(100)));
        auto result = validator.validate(patch, perms);

        REQUIRE_FALSE(result.valid);
    }

    SECTION("component type blocked") {
        NamespacePermissions perms;
        perms.blocked_components = {"Debug"};

        Patch patch(ComponentPatch::set(entity, "Debug", Value(true)));
        auto result = validator.validate(patch, perms);

        REQUIRE_FALSE(result.valid);
    }

    SECTION("hierarchy permission") {
        NamespacePermissions perms;
        perms.can_modify_hierarchy = false;

        Patch patch(HierarchyPatch::set_parent(entity, EntityRef(ns, 2)));
        auto result = validator.validate(patch, perms);

        REQUIRE_FALSE(result.valid);
    }

    SECTION("validate batch") {
        NamespacePermissions perms = NamespacePermissions::full();

        PatchBatch batch;
        batch.push(EntityPatch::create(entity, "A"));
        batch.push(ComponentPatch::add(entity, "Health", Value(100)));

        auto result = validator.validate_batch(batch, perms);
        REQUIRE(result.valid);
    }
}

// =============================================================================
// ValidationResult Tests
// =============================================================================

TEST_CASE("ValidationResult", "[ir][validation]") {
    SECTION("ok result") {
        auto result = ValidationResult::ok();
        REQUIRE(result.valid);
        REQUIRE(result.errors.empty());
    }

    SECTION("failed result") {
        auto result = ValidationResult::failed("Test error");
        REQUIRE_FALSE(result.valid);
        REQUIRE(result.first_error() == "Test error");
    }

    SECTION("field error") {
        auto result = ValidationResult::field_error("health.current", "Out of range");
        REQUIRE_FALSE(result.valid);
        REQUIRE(result.first_error() == "health.current: Out of range");
    }

    SECTION("merge results") {
        auto r1 = ValidationResult::ok();
        auto r2 = ValidationResult::failed("Error 1");
        auto r3 = ValidationResult::field_error("field", "Error 2");

        r1.merge(r2);
        REQUIRE_FALSE(r1.valid);

        r1.merge(r3);
        REQUIRE(r1.errors.size() == 2);
    }

    SECTION("all errors") {
        ValidationResult result;
        result.add_error("a", "Error A");
        result.add_error("b", "Error B");

        auto all = result.all_errors();
        REQUIRE(all.size() == 2);
    }
}
