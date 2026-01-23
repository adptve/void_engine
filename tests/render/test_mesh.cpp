// void_render Mesh tests - testing available APIs

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/render/mesh.hpp>

using namespace void_render;
using Catch::Matchers::WithinAbs;

TEST_CASE("Vertex structure", "[render][mesh]") {
    SECTION("vertex size is correct") {
        REQUIRE(sizeof(Vertex) == 80);
        REQUIRE(VERTEX_SIZE == 80);
    }

    SECTION("vertex default construction") {
        Vertex v;
        REQUIRE_THAT(v.position[0], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(v.position[1], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(v.position[2], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(v.normal[0], WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(v.normal[1], WithinAbs(1.0f, 0.001f));  // Default up
        REQUIRE_THAT(v.normal[2], WithinAbs(0.0f, 0.001f));
    }

    SECTION("vertex construction with position") {
        Vertex v(1.0f, 2.0f, 3.0f);
        REQUIRE_THAT(v.position[0], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(v.position[1], WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(v.position[2], WithinAbs(3.0f, 0.001f));
    }

    SECTION("vertex construction with position and normal") {
        Vertex v(1.0f, 2.0f, 3.0f, 0.0f, 0.0f, 1.0f);
        REQUIRE_THAT(v.position[0], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(v.normal[2], WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("MeshData creation", "[render][mesh]") {
    SECTION("empty mesh") {
        MeshData mesh;
        REQUIRE(mesh.vertex_count() == 0);
        REQUIRE(mesh.index_count() == 0);
    }

    SECTION("quad primitive") {
        auto quad = MeshData::quad();
        REQUIRE(quad.vertex_count() == 4);
        REQUIRE(quad.index_count() == 6);
        REQUIRE(quad.is_indexed());
    }

    SECTION("plane primitive") {
        auto plane = MeshData::plane(10.0f, 4);  // size=10, subdivisions=4
        REQUIRE(plane.vertex_count() == 25);  // 5x5 vertices
        REQUIRE(plane.index_count() == 96);   // 4x4 quads * 2 tris * 3 indices
    }

    SECTION("cube primitive") {
        auto cube = MeshData::cube(1.0f);
        REQUIRE(cube.vertex_count() == 24);  // 6 faces * 4 vertices
        REQUIRE(cube.index_count() == 36);   // 6 faces * 2 tris * 3 indices
    }

    SECTION("sphere primitive") {
        auto sphere = MeshData::sphere(1.0f, 16, 16);
        REQUIRE(sphere.vertex_count() > 0);
        REQUIRE(sphere.index_count() > 0);
    }
}

TEST_CASE("Index format utilities", "[render][mesh]") {
    SECTION("index sizes") {
        REQUIRE(index_size(IndexFormat::U16) == 2);
        REQUIRE(index_size(IndexFormat::U32) == 4);
    }
}
