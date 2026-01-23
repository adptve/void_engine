// void_ir Value tests

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <void_engine/ir/ir.hpp>

using namespace void_ir;

// =============================================================================
// Value Type Tests
// =============================================================================

TEST_CASE("Value construction", "[ir][value]") {
    SECTION("default is null") {
        Value v;
        REQUIRE(v.is_null());
        REQUIRE(v.type() == ValueType::Null);
    }

    SECTION("bool") {
        Value v(true);
        REQUIRE(v.is_bool());
        REQUIRE(v.as_bool() == true);

        Value v2(false);
        REQUIRE(v2.as_bool() == false);
    }

    SECTION("int") {
        Value v(42);
        REQUIRE(v.is_int());
        REQUIRE(v.as_int() == 42);

        Value v2(std::int64_t(-100));
        REQUIRE(v2.as_int() == -100);
    }

    SECTION("float") {
        Value v(3.14f);
        REQUIRE(v.is_float());
        REQUIRE(v.as_float() == Catch::Approx(3.14).epsilon(0.01));

        Value v2(2.718281828);
        REQUIRE(v2.as_float() == Catch::Approx(2.718281828));
    }

    SECTION("string") {
        Value v("hello");
        REQUIRE(v.is_string());
        REQUIRE(v.as_string() == "hello");

        Value v2(std::string("world"));
        REQUIRE(v2.as_string() == "world");
    }

    SECTION("Vec2") {
        Value v(Vec2{1.0f, 2.0f});
        REQUIRE(v.is_vec2());
        REQUIRE(v.as_vec2().x == 1.0f);
        REQUIRE(v.as_vec2().y == 2.0f);
    }

    SECTION("Vec3") {
        Value v(Vec3{1.0f, 2.0f, 3.0f});
        REQUIRE(v.is_vec3());
        REQUIRE(v.as_vec3().x == 1.0f);
        REQUIRE(v.as_vec3().y == 2.0f);
        REQUIRE(v.as_vec3().z == 3.0f);
    }

    SECTION("Vec4") {
        Value v(Vec4{1.0f, 2.0f, 3.0f, 4.0f});
        REQUIRE(v.is_vec4());
        REQUIRE(v.as_vec4().x == 1.0f);
        REQUIRE(v.as_vec4().w == 4.0f);
    }

    SECTION("array") {
        Value v = Value::array({Value(1), Value(2), Value(3)});
        REQUIRE(v.is_array());
        REQUIRE(v.size() == 3);
        REQUIRE(v[0].as_int() == 1);
        REQUIRE(v[2].as_int() == 3);
    }

    SECTION("object") {
        Value v = Value::empty_object();
        v["name"] = Value("test");
        v["count"] = Value(42);

        REQUIRE(v.is_object());
        REQUIRE(v.contains("name"));
        REQUIRE(v["name"].as_string() == "test");
        REQUIRE(v["count"].as_int() == 42);
    }

    SECTION("entity ref") {
        Value v = Value::entity_ref(1, 100);
        REQUIRE(v.is_entity_ref());
        REQUIRE(v.as_entity_ref().namespace_id == 1);
        REQUIRE(v.as_entity_ref().entity_id == 100);
    }

    SECTION("asset ref") {
        Value v = Value::asset_path("textures/player.png");
        REQUIRE(v.is_asset_ref());
        REQUIRE(v.as_asset_ref().path == "textures/player.png");

        Value v2 = Value::asset_uuid(12345);
        REQUIRE(v2.as_asset_ref().uuid == 12345);
    }
}

TEST_CASE("Value numeric coercion", "[ir][value]") {
    SECTION("is_numeric") {
        Value i(42);
        Value f(3.14);
        Value s("not a number");

        REQUIRE(i.is_numeric());
        REQUIRE(f.is_numeric());
        REQUIRE_FALSE(s.is_numeric());
    }

    SECTION("as_numeric converts int") {
        Value v(42);
        REQUIRE(v.as_numeric() == Catch::Approx(42.0));
    }
}

TEST_CASE("Value type checking", "[ir][value]") {
    Value null_v;
    Value bool_v(true);
    Value int_v(42);
    Value float_v(3.14);
    Value string_v("test");
    Value vec3_v(Vec3{0, 0, 0});
    Value array_v = Value::empty_array();
    Value object_v = Value::empty_object();

    REQUIRE(null_v.type_name() == std::string("Null"));
    REQUIRE(bool_v.type_name() == std::string("Bool"));
    REQUIRE(int_v.type_name() == std::string("Int"));
    REQUIRE(float_v.type_name() == std::string("Float"));
    REQUIRE(string_v.type_name() == std::string("String"));
    REQUIRE(vec3_v.type_name() == std::string("Vec3"));
    REQUIRE(array_v.type_name() == std::string("Array"));
    REQUIRE(object_v.type_name() == std::string("Object"));
}

TEST_CASE("Value optional accessors", "[ir][value]") {
    Value v(42);

    REQUIRE(v.try_int().has_value());
    REQUIRE(*v.try_int() == 42);
    REQUIRE_FALSE(v.try_float().has_value());
    REQUIRE_FALSE(v.try_bool().has_value());
    REQUIRE(v.try_string() == nullptr);
}

TEST_CASE("Value comparison", "[ir][value]") {
    SECTION("same types") {
        REQUIRE(Value(42) == Value(42));
        REQUIRE(Value(42) != Value(43));
        REQUIRE(Value("hello") == Value("hello"));
        REQUIRE(Value(true) == Value(true));
    }

    SECTION("different types") {
        REQUIRE(Value(42) != Value(42.0));  // int vs float
        REQUIRE(Value("42") != Value(42));
    }

    SECTION("vec types") {
        REQUIRE(Value(Vec3{1, 2, 3}) == Value(Vec3{1, 2, 3}));
        REQUIRE(Value(Vec3{1, 2, 3}) != Value(Vec3{1, 2, 4}));
    }
}

TEST_CASE("Value clone", "[ir][value]") {
    Value original = Value::empty_object();
    original["nested"] = Value::array({Value(1), Value(2)});
    original["name"] = Value("test");

    Value cloned = original.clone();

    REQUIRE(cloned == original);
    REQUIRE(cloned["name"].as_string() == "test");
    REQUIRE(cloned["nested"].size() == 2);
}
