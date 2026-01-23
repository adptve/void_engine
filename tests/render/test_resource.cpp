// void_render Resource tests - testing available APIs

#include <catch2/catch_test_macros.hpp>
#include <void_engine/render/resource.hpp>

using namespace void_render;

TEST_CASE("ResourceId generation", "[render][resource]") {
    SECTION("from hash produces unique IDs") {
        auto id1 = ResourceId::from_hash("texture1");
        auto id2 = ResourceId::from_hash("texture2");
        auto id3 = ResourceId::from_hash("texture1");

        REQUIRE(id1.value != id2.value);
        REQUIRE(id1.value == id3.value);  // Same name = same hash
    }

    SECTION("from_name is alias for from_hash") {
        auto id1 = ResourceId::from_hash("test");
        auto id2 = ResourceId::from_name("test");
        REQUIRE(id1.value == id2.value);
    }

    SECTION("sequential IDs are unique") {
        auto id1 = ResourceId::sequential();
        auto id2 = ResourceId::sequential();
        auto id3 = ResourceId::sequential();

        REQUIRE(id1.value != id2.value);
        REQUIRE(id2.value != id3.value);
    }

    SECTION("invalid ID check") {
        ResourceId invalid;
        REQUIRE_FALSE(invalid.is_valid());

        auto valid = ResourceId::from_hash("test");
        REQUIRE(valid.is_valid());
    }
}

TEST_CASE("TextureFormat properties", "[render][resource]") {
    SECTION("format byte sizes") {
        REQUIRE(texture_format_bytes(TextureFormat::R8Unorm) == 1);
        REQUIRE(texture_format_bytes(TextureFormat::Rg8Unorm) == 2);
        REQUIRE(texture_format_bytes(TextureFormat::Rgba8Unorm) == 4);
        REQUIRE(texture_format_bytes(TextureFormat::Rgba16Float) == 8);
        REQUIRE(texture_format_bytes(TextureFormat::Rgba32Float) == 16);
    }

    SECTION("bytes_per_pixel is same as texture_format_bytes") {
        REQUIRE(bytes_per_pixel(TextureFormat::R8Unorm) == texture_format_bytes(TextureFormat::R8Unorm));
    }

    SECTION("depth format detection") {
        REQUIRE(is_depth_format(TextureFormat::Depth16Unorm));
        REQUIRE(is_depth_format(TextureFormat::Depth24Plus));
        REQUIRE(is_depth_format(TextureFormat::Depth32Float));
        REQUIRE(is_depth_format(TextureFormat::Depth24PlusStencil8));
        REQUIRE(is_depth_format(TextureFormat::Depth32FloatStencil8));

        REQUIRE_FALSE(is_depth_format(TextureFormat::Rgba8Unorm));
        REQUIRE_FALSE(is_depth_format(TextureFormat::R8Unorm));
    }

    SECTION("stencil format detection") {
        REQUIRE(is_stencil_format(TextureFormat::Depth24PlusStencil8));
        REQUIRE(is_stencil_format(TextureFormat::Depth32FloatStencil8));

        REQUIRE_FALSE(is_stencil_format(TextureFormat::Depth32Float));
        REQUIRE_FALSE(is_stencil_format(TextureFormat::Rgba8Unorm));
    }

    SECTION("has_stencil detection") {
        REQUIRE(has_stencil(TextureFormat::Depth24PlusStencil8));
        REQUIRE(has_stencil(TextureFormat::Depth32FloatStencil8));

        REQUIRE_FALSE(has_stencil(TextureFormat::Depth32Float));
    }

    SECTION("compressed format detection") {
        REQUIRE(is_compressed_format(TextureFormat::Bc1RgbaUnorm));
        REQUIRE(is_compressed_format(TextureFormat::Bc3RgbaUnorm));
        REQUIRE(is_compressed_format(TextureFormat::Bc7RgbaUnorm));

        REQUIRE_FALSE(is_compressed_format(TextureFormat::Rgba8Unorm));
    }

    SECTION("sRGB format detection") {
        REQUIRE(is_srgb_format(TextureFormat::Rgba8UnormSrgb));
        REQUIRE(is_srgb_format(TextureFormat::Bgra8UnormSrgb));
        REQUIRE(is_srgb_format(TextureFormat::Bc1RgbaUnormSrgb));

        REQUIRE_FALSE(is_srgb_format(TextureFormat::Rgba8Unorm));
    }
}

TEST_CASE("TextureDesc creation", "[render][resource]") {
    SECTION("2D texture descriptor") {
        auto desc = TextureDesc::texture_2d(1024, 512, TextureFormat::Rgba8Unorm);

        REQUIRE(desc.size[0] == 1024);
        REQUIRE(desc.size[1] == 512);
        REQUIRE(desc.size[2] == 1);
        REQUIRE(desc.dimension == TextureDimension::D2);
        REQUIRE(desc.format == TextureFormat::Rgba8Unorm);
    }

    SECTION("depth texture descriptor") {
        auto desc = TextureDesc::depth_buffer(1920, 1080);

        REQUIRE(desc.dimension == TextureDimension::D2);
        REQUIRE(desc.format == TextureFormat::Depth32Float);
    }

    SECTION("render target descriptor") {
        auto desc = TextureDesc::render_target(1920, 1080, TextureFormat::Rgba16Float);

        REQUIRE(desc.size[0] == 1920);
        REQUIRE(desc.size[1] == 1080);
    }
}

TEST_CASE("SamplerDesc creation", "[render][resource]") {
    SECTION("linear sampler") {
        auto sampler = SamplerDesc::linear();

        REQUIRE(sampler.mag_filter == FilterMode::Linear);
        REQUIRE(sampler.min_filter == FilterMode::Linear);
        REQUIRE(sampler.mipmap_filter == FilterMode::Linear);
    }

    SECTION("nearest sampler") {
        auto sampler = SamplerDesc::nearest();

        REQUIRE(sampler.mag_filter == FilterMode::Nearest);
        REQUIRE(sampler.min_filter == FilterMode::Nearest);
    }

    SECTION("shadow sampler") {
        auto sampler = SamplerDesc::shadow();

        REQUIRE(sampler.compare.has_value());
        REQUIRE(sampler.compare.value() == CompareFunction::LessEqual);
    }
}

TEST_CASE("ClearValue creation", "[render][resource]") {
    SECTION("color clear value") {
        auto clear = ClearValue::with_color(0.5f, 0.25f, 0.75f, 1.0f);

        REQUIRE(clear.type == ClearValue::Type::Color);
        REQUIRE(clear.color[0] > 0.49f);
        REQUIRE(clear.color[0] < 0.51f);
    }

    SECTION("depth clear value") {
        auto clear = ClearValue::depth_value(0.0f);

        REQUIRE(clear.type == ClearValue::Type::Depth);
    }

    SECTION("depth-stencil clear value") {
        auto clear = ClearValue::depth_stencil_value(1.0f, 128);

        REQUIRE(clear.type == ClearValue::Type::DepthStencil);
    }
}
