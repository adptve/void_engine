// void_core TypeRegistry tests

#include <catch2/catch_test_macros.hpp>
#include <void_engine/core/type_registry.hpp>
#include <string>
#include <vector>

using namespace void_core;

// Test types
struct SimpleData {
    int x = 0;
    float y = 0.0f;
};

struct ComplexData {
    std::string name;
    std::vector<int> values;

    ComplexData() = default;
    ComplexData(std::string n, std::vector<int> v)
        : name(std::move(n)), values(std::move(v)) {}
};

struct NonDefaultConstructible {
    int value;
    explicit NonDefaultConstructible(int v) : value(v) {}
};

// =============================================================================
// TypeInfo Tests
// =============================================================================

TEST_CASE("TypeInfo::of", "[core][type_registry]") {
    SECTION("simple type") {
        TypeInfo info = TypeInfo::of<int>();
        REQUIRE(info.size == sizeof(int));
        REQUIRE(info.align == alignof(int));
        REQUIRE_FALSE(info.needs_drop);  // Trivially destructible
    }

    SECTION("struct type") {
        TypeInfo info = TypeInfo::of<SimpleData>();
        REQUIRE(info.size == sizeof(SimpleData));
        REQUIRE(info.align == alignof(SimpleData));
    }

    SECTION("complex type needs drop") {
        TypeInfo info = TypeInfo::of<std::string>();
        REQUIRE(info.needs_drop);  // Has non-trivial destructor
    }

    SECTION("with readable name") {
        TypeInfo info = TypeInfo::of<SimpleData>().with_name("SimpleData");
        REQUIRE(info.name == "SimpleData");
    }
}

TEST_CASE("TypeInfo with schema", "[core][type_registry]") {
    TypeInfo info = TypeInfo::of<int>().with_schema(
        TypeSchema::primitive(PrimitiveType::I32)
    );

    REQUIRE(info.schema.has_value());
    REQUIRE(info.schema->kind == TypeSchema::Kind::Primitive);
    REQUIRE(info.schema->primitive_type == PrimitiveType::I32);
}

// =============================================================================
// TypeSchema Tests
// =============================================================================

TEST_CASE("TypeSchema factory methods", "[core][type_registry]") {
    SECTION("primitive") {
        TypeSchema schema = TypeSchema::primitive(PrimitiveType::F64);
        REQUIRE(schema.kind == TypeSchema::Kind::Primitive);
        REQUIRE(schema.primitive_type == PrimitiveType::F64);
    }

    SECTION("struct") {
        std::vector<FieldInfo> fields;
        fields.emplace_back("x", 0, std::make_shared<TypeSchema>(TypeSchema::primitive(PrimitiveType::I32)));
        fields.emplace_back("y", 4, std::make_shared<TypeSchema>(TypeSchema::primitive(PrimitiveType::F32)));

        TypeSchema schema = TypeSchema::structure(std::move(fields));
        REQUIRE(schema.kind == TypeSchema::Kind::Struct);
        REQUIRE(schema.fields.size() == 2);
        REQUIRE(schema.fields[0].name == "x");
        REQUIRE(schema.fields[1].name == "y");
    }

    SECTION("array") {
        auto elem = std::make_shared<TypeSchema>(TypeSchema::primitive(PrimitiveType::I32));
        TypeSchema schema = TypeSchema::array(elem);
        REQUIRE(schema.kind == TypeSchema::Kind::Array);
        REQUIRE(schema.element_type != nullptr);
    }

    SECTION("optional") {
        auto inner = std::make_shared<TypeSchema>(TypeSchema::primitive(PrimitiveType::String));
        TypeSchema schema = TypeSchema::optional(inner);
        REQUIRE(schema.kind == TypeSchema::Kind::Optional);
        REQUIRE(schema.element_type != nullptr);
    }

    SECTION("map") {
        auto key = std::make_shared<TypeSchema>(TypeSchema::primitive(PrimitiveType::String));
        auto value = std::make_shared<TypeSchema>(TypeSchema::primitive(PrimitiveType::I32));
        TypeSchema schema = TypeSchema::map(key, value);
        REQUIRE(schema.kind == TypeSchema::Kind::Map);
        REQUIRE(schema.key_type != nullptr);
        REQUIRE(schema.value_type != nullptr);
    }

    SECTION("opaque") {
        TypeSchema schema = TypeSchema::opaque();
        REQUIRE(schema.kind == TypeSchema::Kind::Opaque);
    }
}

TEST_CASE("PrimitiveType names", "[core][type_registry]") {
    REQUIRE(std::string(primitive_type_name(PrimitiveType::Bool)) == "bool");
    REQUIRE(std::string(primitive_type_name(PrimitiveType::I32)) == "i32");
    REQUIRE(std::string(primitive_type_name(PrimitiveType::F64)) == "f64");
    REQUIRE(std::string(primitive_type_name(PrimitiveType::String)) == "string");
}

// =============================================================================
// DynType Tests
// =============================================================================

TEST_CASE("DynTypeImpl construction", "[core][type_registry]") {
    auto dyn = std::make_unique<DynTypeImpl<int>>(42);

    REQUIRE(dyn->value() == 42);
    REQUIRE(dyn->type_info().size == sizeof(int));
}

TEST_CASE("make_dyn helper", "[core][type_registry]") {
    auto dyn = make_dyn(std::string("hello"));

    REQUIRE(dyn != nullptr);
    auto* impl = dynamic_cast<DynTypeImpl<std::string>*>(dyn.get());
    REQUIRE(impl != nullptr);
    REQUIRE(impl->value() == "hello");
}

TEST_CASE("DynType clone_box", "[core][type_registry]") {
    auto original = make_dyn(42);
    auto cloned = original->clone_box();

    REQUIRE(cloned != nullptr);

    auto* orig_impl = dynamic_cast<DynTypeImpl<int>*>(original.get());
    auto* clone_impl = dynamic_cast<DynTypeImpl<int>*>(cloned.get());

    REQUIRE(orig_impl->value() == clone_impl->value());

    // Modify clone should not affect original
    clone_impl->value() = 100;
    REQUIRE(orig_impl->value() == 42);
}

TEST_CASE("DynType as_any", "[core][type_registry]") {
    auto dyn = make_dyn(std::string("test"));
    std::any a = dyn->as_any();

    REQUIRE(std::any_cast<std::string>(&a) != nullptr);
    REQUIRE(std::any_cast<std::string>(a) == "test");
}

TEST_CASE("DynType downcast", "[core][type_registry]") {
    auto dyn = make_dyn(42);

    SECTION("successful downcast") {
        int* ptr = dyn->downcast<int>();
        REQUIRE(ptr != nullptr);
        REQUIRE(*ptr == 42);
    }

    SECTION("failed downcast") {
        float* ptr = dyn->downcast<float>();
        REQUIRE(ptr == nullptr);
    }

    SECTION("const downcast") {
        const DynType* const_dyn = dyn.get();
        const int* ptr = const_dyn->downcast<int>();
        REQUIRE(ptr != nullptr);
        REQUIRE(*ptr == 42);
    }
}

// =============================================================================
// TypeRegistry Tests
// =============================================================================

TEST_CASE("TypeRegistry construction", "[core][type_registry]") {
    TypeRegistry registry;
    REQUIRE(registry.is_empty());
    REQUIRE(registry.len() == 0);
}

TEST_CASE("TypeRegistry register_type", "[core][type_registry]") {
    TypeRegistry registry;

    registry.register_type<SimpleData>();

    REQUIRE(registry.len() == 1);
    REQUIRE(registry.contains<SimpleData>());
}

TEST_CASE("TypeRegistry register_with_name", "[core][type_registry]") {
    TypeRegistry registry;

    registry.register_with_name<SimpleData>("SimpleData");

    REQUIRE(registry.contains<SimpleData>());
    REQUIRE(registry.contains_name("SimpleData"));

    const TypeInfo* info = registry.get_by_name("SimpleData");
    REQUIRE(info != nullptr);
    REQUIRE(info->name == "SimpleData");
}

TEST_CASE("TypeRegistry get", "[core][type_registry]") {
    TypeRegistry registry;
    registry.register_with_name<int>("int");

    SECTION("by type") {
        const TypeInfo* info = registry.get<int>();
        REQUIRE(info != nullptr);
        REQUIRE(info->size == sizeof(int));
    }

    SECTION("by type_index") {
        const TypeInfo* info = registry.get(std::type_index(typeid(int)));
        REQUIRE(info != nullptr);
    }

    SECTION("by name") {
        const TypeInfo* info = registry.get_by_name("int");
        REQUIRE(info != nullptr);
    }

    SECTION("not registered") {
        const TypeInfo* info = registry.get<float>();
        REQUIRE(info == nullptr);
    }
}

TEST_CASE("TypeRegistry create", "[core][type_registry]") {
    TypeRegistry registry;
    registry.register_with_name<SimpleData>("SimpleData");

    SECTION("by type") {
        auto instance = registry.create<SimpleData>();
        REQUIRE(instance != nullptr);

        auto* data = instance->downcast<SimpleData>();
        REQUIRE(data != nullptr);
        REQUIRE(data->x == 0);  // Default constructed
    }

    SECTION("by name") {
        auto instance = registry.create_by_name("SimpleData");
        REQUIRE(instance != nullptr);
    }

    SECTION("not registered") {
        auto instance = registry.create<float>();
        REQUIRE(instance == nullptr);
    }
}

TEST_CASE("TypeRegistry register_with_info", "[core][type_registry]") {
    TypeRegistry registry;

    TypeInfo info = TypeInfo::of<NonDefaultConstructible>()
        .with_name("NonDefaultConstructible");

    registry.register_with_info(info);

    REQUIRE(registry.contains<NonDefaultConstructible>());
    REQUIRE(registry.contains_name("NonDefaultConstructible"));

    // Cannot create without constructor
    auto instance = registry.create<NonDefaultConstructible>();
    REQUIRE(instance == nullptr);
}

TEST_CASE("TypeRegistry clear", "[core][type_registry]") {
    TypeRegistry registry;
    registry.register_type<int>();
    registry.register_type<float>();

    REQUIRE(registry.len() == 2);

    registry.clear();

    REQUIRE(registry.is_empty());
    REQUIRE_FALSE(registry.contains<int>());
}

TEST_CASE("TypeRegistry for_each", "[core][type_registry]") {
    TypeRegistry registry;
    registry.register_with_name<int>("int");
    registry.register_with_name<float>("float");
    registry.register_with_name<double>("double");

    std::vector<std::string> names;
    registry.for_each([&names](const TypeInfo& info) {
        names.push_back(info.name);
    });

    REQUIRE(names.size() == 3);
}

TEST_CASE("TypeRegistry get_result", "[core][type_registry]") {
    TypeRegistry registry;
    registry.register_with_name<int>("int");

    SECTION("success") {
        auto result = registry.get_result(std::type_index(typeid(int)));
        REQUIRE(result.is_ok());
        REQUIRE(result.value().get().name == "int");
    }

    SECTION("not registered") {
        auto result = registry.get_result(std::type_index(typeid(float)));
        REQUIRE(result.is_err());
        REQUIRE(result.error().code() == ErrorCode::NotFound);
    }

    SECTION("by name success") {
        auto result = registry.get_result_by_name("int");
        REQUIRE(result.is_ok());
    }

    SECTION("by name not found") {
        auto result = registry.get_result_by_name("unknown");
        REQUIRE(result.is_err());
    }
}

TEST_CASE("TypeRegistry chain registration", "[core][type_registry]") {
    TypeRegistry registry;

    registry
        .register_with_name<int>("int")
        .register_with_name<float>("float")
        .register_with_name<double>("double");

    REQUIRE(registry.len() == 3);
    REQUIRE(registry.contains_name("int"));
    REQUIRE(registry.contains_name("float"));
    REQUIRE(registry.contains_name("double"));
}
