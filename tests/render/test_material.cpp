#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <void_engine/render/material.hpp>

using namespace void_render;
using Catch::Matchers::WithinAbs;

TEST_CASE("MaterialId", "[render][material]") {
    SECTION("default is invalid") {
        MaterialId id;
        REQUIRE_FALSE(id.is_valid());
    }

    SECTION("explicit construction is valid") {
        MaterialId id(5);
        REQUIRE(id.is_valid());
        REQUIRE(id.index == 5);
    }

    SECTION("invalid factory") {
        auto invalid = MaterialId::invalid();
        REQUIRE_FALSE(invalid.is_valid());
    }
}

TEST_CASE("GpuMaterial size", "[render][material]") {
    SECTION("material is 256 bytes") {
        REQUIRE(sizeof(GpuMaterial) == 256);
        REQUIRE(GpuMaterial::SIZE == 256);
    }
}

TEST_CASE("GpuMaterial factory methods", "[render][material]") {
    SECTION("pbr_default") {
        auto mat = GpuMaterial::pbr_default();
        REQUIRE_THAT(mat.base_color[0], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(mat.base_color[3], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(mat.roughness, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(mat.metallic, WithinAbs(0.0f, 0.001f));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_RECEIVES_SHADOWS));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_CASTS_SHADOWS));
    }

    SECTION("metallic material") {
        auto mat = GpuMaterial::make_metallic({1.0f, 0.8f, 0.2f}, 0.2f);
        REQUIRE_THAT(mat.metallic, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(mat.roughness, WithinAbs(0.2f, 0.001f));
        REQUIRE_THAT(mat.base_color[0], WithinAbs(1.0f, 0.001f));
    }

    SECTION("dielectric material") {
        auto mat = GpuMaterial::dielectric({0.5f, 0.5f, 0.5f}, 0.7f);
        REQUIRE_THAT(mat.metallic, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(mat.roughness, WithinAbs(0.7f, 0.001f));
    }

    SECTION("emissive material") {
        auto mat = GpuMaterial::make_emissive({1.0f, 0.0f, 0.0f}, 5.0f);
        REQUIRE_THAT(mat.emissive[0], WithinAbs(5.0f, 0.001f));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_UNLIT));
    }

    SECTION("glass material") {
        auto mat = GpuMaterial::glass(1.5f);
        REQUIRE_THAT(mat.transmission, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(mat.ior, WithinAbs(1.5f, 0.001f));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_HAS_TRANSMISSION));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_ALPHA_BLEND));
    }

    SECTION("unlit material") {
        auto mat = GpuMaterial::unlit({0.0f, 1.0f, 0.0f});
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_UNLIT));
    }

    SECTION("clearcoat material") {
        auto mat = GpuMaterial::make_clearcoat({0.8f, 0.0f, 0.0f}, 1.0f, 0.1f);
        REQUIRE_THAT(mat.clearcoat, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(mat.clearcoat_roughness, WithinAbs(0.1f, 0.001f));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_HAS_CLEARCOAT));
    }

    SECTION("subsurface material") {
        auto mat = GpuMaterial::make_subsurface({1.0f, 0.8f, 0.7f}, {1.0f, 0.4f, 0.3f}, 0.5f);
        REQUIRE_THAT(mat.subsurface, WithinAbs(0.5f, 0.001f));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_HAS_SUBSURFACE));
    }

    SECTION("fabric material") {
        auto mat = GpuMaterial::fabric({0.5f, 0.5f, 0.5f});
        REQUIRE_THAT(mat.sheen, WithinAbs(1.0f, 0.001f));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_HAS_SHEEN));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_DOUBLE_SIDED));
    }
}

TEST_CASE("GpuMaterial flags", "[render][material]") {
    SECTION("set and check flags") {
        GpuMaterial mat;
        mat.set_flag(GpuMaterial::FLAG_DOUBLE_SIDED);
        mat.set_flag(GpuMaterial::FLAG_ALPHA_MASK);

        REQUIRE(mat.has_flag(GpuMaterial::FLAG_DOUBLE_SIDED));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_ALPHA_MASK));
        REQUIRE_FALSE(mat.has_flag(GpuMaterial::FLAG_UNLIT));
    }

    SECTION("clear flag") {
        GpuMaterial mat;
        mat.set_flag(GpuMaterial::FLAG_DOUBLE_SIDED);
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_DOUBLE_SIDED));

        mat.set_flag(GpuMaterial::FLAG_DOUBLE_SIDED, false);
        REQUIRE_FALSE(mat.has_flag(GpuMaterial::FLAG_DOUBLE_SIDED));
    }
}

TEST_CASE("GpuMaterial fluent API", "[render][material]") {
    SECTION("chained setters") {
        auto mat = GpuMaterial::pbr_default()
            .with_base_color(1.0f, 0.0f, 0.0f, 1.0f)
            .with_metallic(0.8f)
            .with_roughness(0.3f)
            .with_emissive(0.5f, 0.5f, 0.5f);

        REQUIRE_THAT(mat.base_color[0], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(mat.metallic, WithinAbs(0.8f, 0.001f));
        REQUIRE_THAT(mat.roughness, WithinAbs(0.3f, 0.001f));
        REQUIRE_THAT(mat.emissive[0], WithinAbs(0.5f, 0.001f));
    }

    SECTION("clearcoat setter") {
        auto mat = GpuMaterial::pbr_default()
            .with_clearcoat(0.5f, 0.1f);

        REQUIRE_THAT(mat.clearcoat, WithinAbs(0.5f, 0.001f));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_HAS_CLEARCOAT));
    }

    SECTION("transmission setter") {
        auto mat = GpuMaterial::pbr_default()
            .with_transmission(0.9f, 1.45f);

        REQUIRE_THAT(mat.transmission, WithinAbs(0.9f, 0.001f));
        REQUIRE_THAT(mat.ior, WithinAbs(1.45f, 0.001f));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_HAS_TRANSMISSION));
    }

    SECTION("alpha mask setter") {
        auto mat = GpuMaterial::pbr_default()
            .with_alpha_mask(0.3f);

        REQUIRE_THAT(mat.alpha_cutoff, WithinAbs(0.3f, 0.001f));
        REQUIRE(mat.has_flag(GpuMaterial::FLAG_ALPHA_MASK));
        REQUIRE_FALSE(mat.has_flag(GpuMaterial::FLAG_ALPHA_BLEND));
    }

    SECTION("alpha blend setter") {
        auto mat = GpuMaterial::pbr_default()
            .with_alpha_blend();

        REQUIRE(mat.has_flag(GpuMaterial::FLAG_ALPHA_BLEND));
        REQUIRE_FALSE(mat.has_flag(GpuMaterial::FLAG_ALPHA_MASK));
    }
}

TEST_CASE("MaterialBuffer", "[render][material]") {
    MaterialBuffer buffer;

    SECTION("starts empty") {
        REQUIRE(buffer.empty());
        REQUIRE(buffer.count() == 0);
    }

    SECTION("add material") {
        auto mat = GpuMaterial::pbr_default();
        auto id = buffer.add(mat);

        REQUIRE(id.is_valid());
        REQUIRE(buffer.count() == 1);
        REQUIRE_FALSE(buffer.empty());
    }

    SECTION("get material") {
        auto mat = GpuMaterial::make_metallic({1.0f, 0.5f, 0.0f});
        auto id = buffer.add(mat);

        const auto* retrieved = buffer.get(id);
        REQUIRE(retrieved != nullptr);
        REQUIRE_THAT(retrieved->base_color[0], WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(retrieved->metallic, WithinAbs(1.0f, 0.001f));
    }

    SECTION("get mutable material") {
        auto mat = GpuMaterial::pbr_default();
        auto id = buffer.add(mat);

        auto* mutable_mat = buffer.get_mut(id);
        REQUIRE(mutable_mat != nullptr);

        mutable_mat->roughness = 0.9f;

        const auto* retrieved = buffer.get(id);
        REQUIRE_THAT(retrieved->roughness, WithinAbs(0.9f, 0.001f));
    }

    SECTION("update material") {
        auto mat1 = GpuMaterial::pbr_default();
        auto id = buffer.add(mat1);

        auto mat2 = GpuMaterial::make_metallic({1.0f, 0.0f, 0.0f});
        bool success = buffer.update(id, mat2);

        REQUIRE(success);
        REQUIRE_THAT(buffer.get(id)->metallic, WithinAbs(1.0f, 0.001f));
    }

    SECTION("add with asset ID") {
        auto mat = GpuMaterial::pbr_default();
        auto id = buffer.add(12345, mat);

        REQUIRE(id.is_valid());

        auto found = buffer.get_by_asset(12345);
        REQUIRE(found.has_value());
        REQUIRE(found->index == id.index);
    }

    SECTION("ensure default") {
        REQUIRE(buffer.empty());

        buffer.ensure_default();
        REQUIRE(buffer.count() == 1);

        buffer.ensure_default();  // Should not add another
        REQUIRE(buffer.count() == 1);
    }

    SECTION("clear buffer") {
        buffer.add(GpuMaterial::pbr_default());
        buffer.add(GpuMaterial::make_metallic({1, 1, 1}));
        REQUIRE(buffer.count() == 2);

        buffer.clear();
        REQUIRE(buffer.empty());
    }

    SECTION("buffer limits") {
        for (std::size_t i = 0; i < MAX_MATERIALS; ++i) {
            auto id = buffer.add(GpuMaterial::pbr_default());
            REQUIRE(id.is_valid());
        }

        REQUIRE(buffer.is_full());

        // Adding more should fail
        auto overflow_id = buffer.add(GpuMaterial::pbr_default());
        REQUIRE_FALSE(overflow_id.is_valid());
    }

    SECTION("data pointer and size") {
        buffer.add(GpuMaterial::pbr_default());
        buffer.add(GpuMaterial::make_metallic({1, 0, 0}));

        REQUIRE(buffer.data() != nullptr);
        REQUIRE(buffer.data_size() == 2 * sizeof(GpuMaterial));
    }
}
