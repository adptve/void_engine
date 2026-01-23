/// @file test_binding.cpp
/// @brief Tests for void_shader binding system

#include <catch2/catch_test_macros.hpp>
#include <void_engine/shader/binding.hpp>
#include <string>

using namespace void_shader;

// =============================================================================
// BindingType Tests
// =============================================================================

TEST_CASE("BindingType: names are correct", "[shader][binding]") {
    REQUIRE(std::string(binding_type_name(BindingType::UniformBuffer)) == "UniformBuffer");
    REQUIRE(std::string(binding_type_name(BindingType::StorageBuffer)) == "StorageBuffer");
    REQUIRE(std::string(binding_type_name(BindingType::ReadOnlyStorageBuffer)) == "ReadOnlyStorageBuffer");
    REQUIRE(std::string(binding_type_name(BindingType::Sampler)) == "Sampler");
    REQUIRE(std::string(binding_type_name(BindingType::SampledTexture)) == "SampledTexture");
    REQUIRE(std::string(binding_type_name(BindingType::StorageTexture)) == "StorageTexture");
    REQUIRE(std::string(binding_type_name(BindingType::CombinedImageSampler)) == "CombinedImageSampler");
}

// =============================================================================
// VertexFormat Tests
// =============================================================================

TEST_CASE("VertexFormat: sizes are correct", "[shader][binding]") {
    REQUIRE(vertex_format_size(VertexFormat::Float32) == 4);
    REQUIRE(vertex_format_size(VertexFormat::Float32x2) == 8);
    REQUIRE(vertex_format_size(VertexFormat::Float32x3) == 12);
    REQUIRE(vertex_format_size(VertexFormat::Float32x4) == 16);
    REQUIRE(vertex_format_size(VertexFormat::Sint32x4) == 16);
    REQUIRE(vertex_format_size(VertexFormat::Uint32x4) == 16);
}

// =============================================================================
// BindingInfo Tests
// =============================================================================

TEST_CASE("BindingInfo: default", "[shader][binding]") {
    BindingInfo info;
    REQUIRE_FALSE(info.name.has_value());
    REQUIRE(info.group == 0);
    REQUIRE(info.binding == 0);
    REQUIRE(info.type == BindingType::UniformBuffer);
}

TEST_CASE("BindingInfo: uniform_buffer factory", "[shader][binding]") {
    auto info = BindingInfo::uniform_buffer(0, 1, 64, "u_transform");

    REQUIRE(info.name.has_value());
    REQUIRE(*info.name == "u_transform");
    REQUIRE(info.group == 0);
    REQUIRE(info.binding == 1);
    REQUIRE(info.type == BindingType::UniformBuffer);
    REQUIRE(info.min_binding_size == 64);
}

TEST_CASE("BindingInfo: storage_buffer factory", "[shader][binding]") {
    auto info = BindingInfo::storage_buffer(1, 0, false, "data");

    REQUIRE(info.type == BindingType::StorageBuffer);
    REQUIRE(info.group == 1);
    REQUIRE(info.binding == 0);
}

TEST_CASE("BindingInfo: storage_buffer read_only factory", "[shader][binding]") {
    auto info = BindingInfo::storage_buffer(0, 0, true, "readonly_data");

    REQUIRE(info.type == BindingType::ReadOnlyStorageBuffer);
}

TEST_CASE("BindingInfo: sampler factory", "[shader][binding]") {
    auto info = BindingInfo::sampler(0, 2, "tex_sampler");

    REQUIRE(info.type == BindingType::Sampler);
    REQUIRE(info.binding == 2);
}

TEST_CASE("BindingInfo: texture factory", "[shader][binding]") {
    auto info = BindingInfo::texture(0, 3, TextureDimension::Texture2D, "albedo");

    REQUIRE(info.type == BindingType::SampledTexture);
    REQUIRE(info.texture_dimension == TextureDimension::Texture2D);
}

// =============================================================================
// BindGroupLayout Tests
// =============================================================================

TEST_CASE("BindGroupLayout: default", "[shader][binding]") {
    BindGroupLayout layout;
    REQUIRE(layout.group == 0);
    REQUIRE(layout.bindings.empty());
}

TEST_CASE("BindGroupLayout: construct with group", "[shader][binding]") {
    BindGroupLayout layout(2);
    REQUIRE(layout.group == 2);
}

TEST_CASE("BindGroupLayout: with_binding", "[shader][binding]") {
    BindGroupLayout layout(0);
    layout.with_binding(BindingInfo::uniform_buffer(0, 0, 64, "uniform1"))
          .with_binding(BindingInfo::uniform_buffer(0, 1, 128, "uniform2"));

    REQUIRE(layout.bindings.size() == 2);
}

TEST_CASE("BindGroupLayout: get_binding", "[shader][binding]") {
    BindGroupLayout layout(0);
    layout.with_binding(BindingInfo::uniform_buffer(0, 5, 64, "test"));

    const auto* found = layout.get_binding(5);
    REQUIRE(found != nullptr);
    REQUIRE(found->name.has_value());
    REQUIRE(*found->name == "test");

    const auto* not_found = layout.get_binding(10);
    REQUIRE(not_found == nullptr);
}

TEST_CASE("BindGroupLayout: has_binding", "[shader][binding]") {
    BindGroupLayout layout(0);
    layout.with_binding(BindingInfo::uniform_buffer(0, 0, 64));

    REQUIRE(layout.has_binding(0));
    REQUIRE_FALSE(layout.has_binding(1));
}

TEST_CASE("BindGroupLayout: binding_count", "[shader][binding]") {
    BindGroupLayout layout(0);
    REQUIRE(layout.binding_count() == 0);

    layout.with_binding(BindingInfo::uniform_buffer(0, 0, 64))
          .with_binding(BindingInfo::sampler(0, 1));

    REQUIRE(layout.binding_count() == 2);
}

TEST_CASE("BindGroupLayout: sort_bindings", "[shader][binding]") {
    BindGroupLayout layout(0);
    layout.with_binding(BindingInfo::uniform_buffer(0, 3, 64))
          .with_binding(BindingInfo::uniform_buffer(0, 1, 64))
          .with_binding(BindingInfo::uniform_buffer(0, 2, 64));

    layout.sort_bindings();

    REQUIRE(layout.bindings[0].binding == 1);
    REQUIRE(layout.bindings[1].binding == 2);
    REQUIRE(layout.bindings[2].binding == 3);
}

// =============================================================================
// VertexInput Tests
// =============================================================================

TEST_CASE("VertexInput: default", "[shader][binding]") {
    VertexInput input;
    REQUIRE(input.location == 0);
    REQUIRE_FALSE(input.name.has_value());
    REQUIRE(input.format == VertexFormat::Float32x4);
}

TEST_CASE("VertexInput: construction", "[shader][binding]") {
    VertexInput input(0, VertexFormat::Float32x3, "a_position");

    REQUIRE(input.location == 0);
    REQUIRE(input.format == VertexFormat::Float32x3);
    REQUIRE(input.name.has_value());
    REQUIRE(*input.name == "a_position");
}

TEST_CASE("VertexInput: size", "[shader][binding]") {
    VertexInput input(0, VertexFormat::Float32x3);
    REQUIRE(input.size() == 12);  // 3 * 4 bytes
}

// =============================================================================
// FragmentOutput Tests
// =============================================================================

TEST_CASE("FragmentOutput: default", "[shader][binding]") {
    FragmentOutput output;
    REQUIRE(output.location == 0);
    REQUIRE_FALSE(output.name.has_value());
}

TEST_CASE("FragmentOutput: construction", "[shader][binding]") {
    FragmentOutput output(0, VertexFormat::Float32x4, "o_color");

    REQUIRE(output.location == 0);
    REQUIRE(output.format == VertexFormat::Float32x4);
    REQUIRE(output.name.has_value());
    REQUIRE(*output.name == "o_color");
}

// =============================================================================
// PushConstantRange Tests
// =============================================================================

TEST_CASE("PushConstantRange: default", "[shader][binding]") {
    PushConstantRange range;
    REQUIRE(range.offset == 0);
    REQUIRE(range.size == 0);
}

TEST_CASE("PushConstantRange: construction", "[shader][binding]") {
    PushConstantRange range(ShaderStage::Vertex, 0, 64);

    REQUIRE(range.stages == ShaderStage::Vertex);
    REQUIRE(range.offset == 0);
    REQUIRE(range.size == 64);
}

// =============================================================================
// ShaderReflection Tests
// =============================================================================

TEST_CASE("ShaderReflection: default", "[shader][binding]") {
    ShaderReflection refl;
    REQUIRE(refl.bind_groups.empty());
    REQUIRE(refl.vertex_inputs.empty());
    REQUIRE(refl.fragment_outputs.empty());
    REQUIRE_FALSE(refl.push_constants.has_value());
    REQUIRE(refl.entry_points.empty());
}

TEST_CASE("ShaderReflection: get_bind_group", "[shader][binding]") {
    ShaderReflection refl;

    BindGroupLayout layout(2);
    layout.with_binding(BindingInfo::uniform_buffer(2, 0, 64));
    refl.bind_groups[2] = layout;

    const auto* found = refl.get_bind_group(2);
    REQUIRE(found != nullptr);
    REQUIRE(found->group == 2);

    const auto* not_found = refl.get_bind_group(0);
    REQUIRE(not_found == nullptr);
}

TEST_CASE("ShaderReflection: has_bind_group", "[shader][binding]") {
    ShaderReflection refl;
    refl.bind_groups[0] = BindGroupLayout(0);

    REQUIRE(refl.has_bind_group(0));
    REQUIRE_FALSE(refl.has_bind_group(1));
}

TEST_CASE("ShaderReflection: total_binding_count", "[shader][binding]") {
    ShaderReflection refl;

    BindGroupLayout layout0(0);
    layout0.with_binding(BindingInfo::uniform_buffer(0, 0, 64))
           .with_binding(BindingInfo::sampler(0, 1));
    refl.bind_groups[0] = layout0;

    BindGroupLayout layout1(1);
    layout1.with_binding(BindingInfo::texture(1, 0));
    refl.bind_groups[1] = layout1;

    REQUIRE(refl.total_binding_count() == 3);
}

TEST_CASE("ShaderReflection: get_vertex_input", "[shader][binding]") {
    ShaderReflection refl;

    refl.vertex_inputs.push_back(VertexInput(0, VertexFormat::Float32x3, "position"));
    refl.vertex_inputs.push_back(VertexInput(1, VertexFormat::Float32x2, "texcoord"));

    const auto* found = refl.get_vertex_input(0);
    REQUIRE(found != nullptr);
    REQUIRE(found->name.has_value());
    REQUIRE(*found->name == "position");

    const auto* not_found = refl.get_vertex_input(5);
    REQUIRE(not_found == nullptr);
}

TEST_CASE("ShaderReflection: has_entry_point", "[shader][binding]") {
    ShaderReflection refl;
    refl.entry_points.push_back("main");
    refl.entry_points.push_back("compute_main");

    REQUIRE(refl.has_entry_point("main"));
    REQUIRE(refl.has_entry_point("compute_main"));
    REQUIRE_FALSE(refl.has_entry_point("vertex_main"));
}

TEST_CASE("ShaderReflection: max_bind_group", "[shader][binding]") {
    ShaderReflection refl;

    REQUIRE(refl.max_bind_group() == 0);

    refl.bind_groups[0] = BindGroupLayout(0);
    refl.bind_groups[3] = BindGroupLayout(3);

    REQUIRE(refl.max_bind_group() == 3);
}

TEST_CASE("ShaderReflection: vertex_stride", "[shader][binding]") {
    ShaderReflection refl;

    refl.vertex_inputs.push_back(VertexInput(0, VertexFormat::Float32x3));  // 12 bytes
    refl.vertex_inputs.push_back(VertexInput(1, VertexFormat::Float32x2));  // 8 bytes

    REQUIRE(refl.vertex_stride() == 20);
}

TEST_CASE("ShaderReflection: is_compute", "[shader][binding]") {
    ShaderReflection refl;
    REQUIRE_FALSE(refl.is_compute());

    refl.workgroup_size = std::array<std::uint32_t, 3>{8, 8, 1};
    REQUIRE(refl.is_compute());
}

TEST_CASE("ShaderReflection: merge", "[shader][binding]") {
    ShaderReflection vert;
    vert.vertex_inputs.push_back(VertexInput(0, VertexFormat::Float32x3));
    vert.entry_points.push_back("vert_main");

    BindGroupLayout vert_layout(0);
    vert_layout.with_binding(BindingInfo::uniform_buffer(0, 0, 64));
    vert.bind_groups[0] = vert_layout;

    ShaderReflection frag;
    frag.fragment_outputs.push_back(FragmentOutput(0, VertexFormat::Float32x4));
    frag.entry_points.push_back("frag_main");

    BindGroupLayout frag_layout(0);
    frag_layout.with_binding(BindingInfo::sampler(0, 1));
    frag.bind_groups[0] = frag_layout;

    vert.merge(frag);

    // Should have both entry points
    REQUIRE(vert.has_entry_point("vert_main"));
    REQUIRE(vert.has_entry_point("frag_main"));

    // Should have both vertex inputs and fragment outputs
    REQUIRE(vert.vertex_inputs.size() == 1);
    REQUIRE(vert.fragment_outputs.size() == 1);

    // Should have merged bind groups
    REQUIRE(vert.bind_groups[0].bindings.size() == 2);
}

// =============================================================================
// Bind Group Constants Tests
// =============================================================================

TEST_CASE("bind_group constants", "[shader][binding]") {
    REQUIRE(bind_group::GLOBAL == 0);
    REQUIRE(bind_group::MATERIAL == 1);
    REQUIRE(bind_group::OBJECT == 2);
    REQUIRE(bind_group::CUSTOM == 3);
}
