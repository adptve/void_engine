#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/render/spatial.hpp>

using namespace void_render;
using Catch::Matchers::WithinAbs;

TEST_CASE("Ray", "[render][spatial]") {
    SECTION("default ray") {
        Ray ray;
        REQUIRE_THAT(ray.direction[2], WithinAbs(-1.0f, 0.001f));  // Default: -Z
    }

    SECTION("ray from points") {
        Ray ray = Ray::from_points({0, 0, 0}, {0, 0, -10});
        REQUIRE_THAT(ray.direction[2], WithinAbs(-1.0f, 0.001f));
    }

    SECTION("ray at distance") {
        Ray ray({0, 0, 0}, {1, 0, 0});
        auto point = ray.at(5.0f);
        REQUIRE_THAT(point[0], WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(point[1], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(point[2], WithinAbs(0.0f, 0.001f));
    }

    SECTION("ray direction is normalized") {
        Ray ray({0, 0, 0}, {3, 4, 0});
        float len = std::sqrt(ray.direction[0] * ray.direction[0] +
                             ray.direction[1] * ray.direction[1] +
                             ray.direction[2] * ray.direction[2]);
        REQUIRE_THAT(len, WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("AABB", "[render][spatial]") {
    SECTION("default AABB is invalid") {
        AABB box;
        REQUIRE_FALSE(box.is_valid());
    }

    SECTION("AABB from min/max") {
        AABB box({-1, -2, -3}, {1, 2, 3});
        REQUIRE(box.is_valid());
    }

    SECTION("center calculation") {
        AABB box({0, 0, 0}, {10, 10, 10});
        auto center = box.center();
        REQUIRE_THAT(center[0], WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(center[1], WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(center[2], WithinAbs(5.0f, 0.001f));
    }

    SECTION("extents calculation") {
        AABB box({0, 0, 0}, {10, 20, 30});
        auto ext = box.extents();
        REQUIRE_THAT(ext[0], WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(ext[1], WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(ext[2], WithinAbs(15.0f, 0.001f));
    }

    SECTION("expand to include point") {
        AABB box;
        box.expand({0, 0, 0});
        box.expand({1, 1, 1});

        REQUIRE(box.is_valid());
        REQUIRE_THAT(box.min[0], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(box.max[0], WithinAbs(1.0f, 0.001f));
    }

    SECTION("contains point") {
        AABB box({0, 0, 0}, {10, 10, 10});

        REQUIRE(box.contains({5, 5, 5}));
        REQUIRE(box.contains({0, 0, 0}));
        REQUIRE(box.contains({10, 10, 10}));
        REQUIRE_FALSE(box.contains({-1, 5, 5}));
        REQUIRE_FALSE(box.contains({11, 5, 5}));
    }

    SECTION("intersects other AABB") {
        AABB box1({0, 0, 0}, {10, 10, 10});
        AABB box2({5, 5, 5}, {15, 15, 15});
        AABB box3({20, 20, 20}, {30, 30, 30});

        REQUIRE(box1.intersects(box2));
        REQUIRE(box2.intersects(box1));
        REQUIRE_FALSE(box1.intersects(box3));
    }

    SECTION("ray intersection - hit") {
        AABB box({-1, -1, -1}, {1, 1, 1});
        Ray ray({0, 0, 10}, {0, 0, -1});

        float t = box.ray_intersect(ray);
        REQUIRE(t >= 0.0f);
        REQUIRE_THAT(t, WithinAbs(9.0f, 0.01f));  // Ray hits at z=1
    }

    SECTION("ray intersection - miss") {
        AABB box({-1, -1, -1}, {1, 1, 1});
        Ray ray({10, 10, 10}, {0, 0, -1});  // Ray misses box

        float t = box.ray_intersect(ray);
        REQUIRE(t < 0.0f);
    }

    SECTION("longest axis") {
        AABB box({0, 0, 0}, {10, 5, 3});
        REQUIRE(box.longest_axis() == 0);  // X is longest

        AABB box2({0, 0, 0}, {3, 10, 5});
        REQUIRE(box2.longest_axis() == 1);  // Y is longest
    }
}

TEST_CASE("BoundingSphere", "[render][spatial]") {
    SECTION("from AABB") {
        AABB box({-1, -1, -1}, {1, 1, 1});
        auto sphere = BoundingSphere::from_aabb(box);

        REQUIRE_THAT(sphere.center[0], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(sphere.center[1], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(sphere.center[2], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(sphere.radius, WithinAbs(std::sqrt(3.0f), 0.001f));
    }

    SECTION("contains point") {
        BoundingSphere sphere({0, 0, 0}, 5.0f);

        REQUIRE(sphere.contains({0, 0, 0}));
        REQUIRE(sphere.contains({4, 0, 0}));
        REQUIRE_FALSE(sphere.contains({10, 0, 0}));
    }

    SECTION("ray intersection") {
        BoundingSphere sphere({0, 0, 0}, 1.0f);
        Ray ray({0, 0, 10}, {0, 0, -1});

        float t = sphere.ray_intersect(ray);
        REQUIRE(t >= 0.0f);
        REQUIRE_THAT(t, WithinAbs(9.0f, 0.01f));  // Hit at z=1
    }
}

TEST_CASE("BVH", "[render][spatial]") {
    SECTION("empty BVH") {
        BVH bvh;
        REQUIRE(bvh.node_count() == 0);
        REQUIRE(bvh.primitive_count() == 0);
    }

    SECTION("build with primitives") {
        std::vector<BVHPrimitive> prims;

        for (int i = 0; i < 10; ++i) {
            BVHPrimitive prim;
            float x = static_cast<float>(i);
            prim.bounds = AABB({x, 0, 0}, {x + 1, 1, 1});
            prim.entity_id = static_cast<std::uint64_t>(i);
            prims.push_back(prim);
        }

        BVH bvh;
        bvh.build(std::move(prims));

        REQUIRE(bvh.primitive_count() == 10);
        REQUIRE(bvh.node_count() > 0);
    }

    SECTION("ray intersection") {
        std::vector<BVHPrimitive> prims;

        // Create a primitive at origin
        BVHPrimitive prim;
        prim.bounds = AABB({-1, -1, -1}, {1, 1, 1});
        prim.entity_id = 42;
        prims.push_back(prim);

        BVH bvh;
        bvh.build(std::move(prims));

        // Cast ray toward origin
        Ray ray({0, 0, 10}, {0, 0, -1});
        auto hit = bvh.ray_intersect(ray);

        REQUIRE(hit.has_value());
        REQUIRE(hit->hit);
        REQUIRE(hit->entity_id == 42);
    }

    SECTION("query AABB") {
        std::vector<BVHPrimitive> prims;

        for (int i = 0; i < 5; ++i) {
            BVHPrimitive prim;
            float x = static_cast<float>(i * 10);
            prim.bounds = AABB({x, 0, 0}, {x + 1, 1, 1});
            prim.entity_id = static_cast<std::uint64_t>(i);
            prims.push_back(prim);
        }

        BVH bvh;
        bvh.build(std::move(prims));

        // Query should find entities 0 and 1
        AABB query({-5, -5, -5}, {15, 5, 5});
        std::vector<std::uint64_t> entities;
        bvh.query_aabb(query, entities);

        REQUIRE(entities.size() == 2);
    }
}

TEST_CASE("SpatialHash", "[render][spatial]") {
    SECTION("insert and query") {
        SpatialHash hash(10.0f);

        hash.insert(1, {5, 5, 5});
        hash.insert(2, {7, 7, 7});
        hash.insert(3, {50, 50, 50});

        std::vector<std::uint64_t> results;
        hash.query({0, 0, 0}, 15.0f, results);

        // Should find entities 1 and 2, not 3
        REQUIRE(results.size() == 2);
    }

    SECTION("clear hash") {
        SpatialHash hash(10.0f);
        hash.insert(1, {0, 0, 0});
        hash.insert(2, {5, 5, 5});

        hash.clear();

        std::vector<std::uint64_t> results;
        hash.query({0, 0, 0}, 100.0f, results);
        REQUIRE(results.empty());
    }
}

TEST_CASE("PickingManager", "[render][spatial]") {
    BVH bvh;
    std::vector<BVHPrimitive> prims;

    BVHPrimitive prim;
    prim.bounds = AABB({-1, -1, -5}, {1, 1, -3});
    prim.entity_id = 100;
    prims.push_back(prim);

    bvh.build(std::move(prims));

    PickingManager picker;
    picker.set_bvh(&bvh);

    SECTION("pick with ray") {
        Ray ray({0, 0, 0}, {0, 0, -1});
        auto result = picker.pick_ray(ray);

        REQUIRE(result.hit);
        REQUIRE(result.entity_id == 100);
    }

    SECTION("pick miss") {
        Ray ray({100, 100, 0}, {0, 0, -1});  // Ray misses
        auto result = picker.pick_ray(ray);

        REQUIRE_FALSE(result.hit);
    }
}
