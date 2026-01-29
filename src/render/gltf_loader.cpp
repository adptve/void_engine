/// @file gltf_loader.cpp
/// @brief glTF 2.0 model loading using tinygltf - implements pimpl from header

#include <void_engine/render/gltf_loader.hpp>
#include <void_engine/render/mesh.hpp>
#include <void_engine/render/material.hpp>
#include <void_engine/render/texture.hpp>

#include <spdlog/spdlog.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <stdexcept>

// tinygltf configuration
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE        // We use our own stb_image
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>

namespace void_render {

// =============================================================================
// GltfTransform Implementation
// =============================================================================

std::array<float, 16> GltfTransform::to_matrix() const noexcept {
    // Quaternion to rotation matrix
    float x = rotation[0], y = rotation[1], z = rotation[2], w = rotation[3];
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    float sx = scale[0], sy = scale[1], sz = scale[2];
    float tx = translation[0], ty = translation[1], tz = translation[2];

    return {{
        (1 - (yy + zz)) * sx,  (xy + wz) * sx,         (xz - wy) * sx,         0,
        (xy - wz) * sy,         (1 - (xx + zz)) * sy,  (yz + wx) * sy,         0,
        (xz + wy) * sz,         (yz - wx) * sz,         (1 - (xx + yy)) * sz,  0,
        tx,                      ty,                      tz,                      1
    }};
}

std::array<float, 16> GltfTransform::multiply(
    const std::array<float, 16>& a,
    const std::array<float, 16>& b) noexcept {
    std::array<float, 16> result = {};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            result[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    return result;
}

// =============================================================================
// GltfLoader::Impl
// =============================================================================

class GltfLoader::Impl {
public:
    LoadOptions m_options;
    std::string m_base_path;
    std::string m_last_error;

    // -------------------------------------------------------------------------
    // Main Load Function
    // -------------------------------------------------------------------------

    std::optional<GltfScene> load(const std::string& path, const LoadOptions& options) {
        m_options = options;
        m_base_path = std::filesystem::path(path).parent_path().string();

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ret = false;
        std::string ext = std::filesystem::path(path).extension().string();

        if (ext == ".glb" || ext == ".GLB") {
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
        } else {
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
        }

        if (!warn.empty()) {
            spdlog::warn("glTF warning [{}]: {}", path, warn);
        }

        if (!ret || !err.empty()) {
            m_last_error = err.empty() ? "Failed to load glTF file" : err;
            spdlog::error("glTF error [{}]: {}", path, m_last_error);
            return std::nullopt;
        }

        GltfScene scene;
        scene.source_path = path;

        // Load textures first
        if (options.load_textures) {
            load_textures(model, scene);
        }

        // Load materials
        load_materials(model, scene);

        // Load meshes
        load_meshes(model, scene);

        // Load nodes
        load_nodes(model, scene);

        // Load default scene
        int scene_index = model.defaultScene >= 0 ? model.defaultScene : 0;
        if (scene_index < static_cast<int>(model.scenes.size())) {
            const auto& gltf_scene = model.scenes[scene_index];
            scene.name = gltf_scene.name;
            for (int node_idx : gltf_scene.nodes) {
                scene.root_nodes.push_back(node_idx);
            }
        }

        // Compute world transforms
        for (int root : scene.root_nodes) {
            compute_world_transforms(scene, root, {{
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
            }});
        }

        // Compute scene bounds
        compute_scene_bounds(scene);

        spdlog::info("Loaded glTF: {} ({} meshes, {} materials, {} nodes, {} verts, {} tris)",
                     path, scene.meshes.size(), scene.materials.size(), scene.nodes.size(),
                     scene.total_vertices, scene.total_triangles);

        return scene;
    }

    // -------------------------------------------------------------------------
    // Texture loading
    // -------------------------------------------------------------------------

    void load_textures(const tinygltf::Model& model, GltfScene& scene) {
        scene.textures.reserve(model.textures.size());

        for (const auto& tex : model.textures) {
            GltfTexture gltf_tex;
            gltf_tex.name = tex.name;

            if (tex.source >= 0 && tex.source < static_cast<int>(model.images.size())) {
                const auto& image = model.images[tex.source];

                if (!image.uri.empty()) {
                    gltf_tex.uri = image.uri;
                }

                // Copy image data
                if (!image.image.empty()) {
                    gltf_tex.data.width = image.width;
                    gltf_tex.data.height = image.height;
                    gltf_tex.data.channels = image.component;
                    gltf_tex.data.pixels = image.image;
                }
            }

            // Sampler settings
            if (tex.sampler >= 0 && tex.sampler < static_cast<int>(model.samplers.size())) {
                const auto& sampler = model.samplers[tex.sampler];
                gltf_tex.min_filter = sampler.minFilter;
                gltf_tex.mag_filter = sampler.magFilter;
                gltf_tex.wrap_s = sampler.wrapS;
                gltf_tex.wrap_t = sampler.wrapT;
            }

            scene.textures.push_back(std::move(gltf_tex));
        }
    }

    // -------------------------------------------------------------------------
    // Material loading
    // -------------------------------------------------------------------------

    void load_materials(const tinygltf::Model& model, GltfScene& scene) {
        scene.materials.reserve(model.materials.size());

        for (const auto& mat : model.materials) {
            GltfMaterial gltf_mat;
            gltf_mat.name = mat.name;

            GpuMaterial& gpu = gltf_mat.gpu_material;

            // PBR Metallic Roughness
            const auto& pbr = mat.pbrMetallicRoughness;

            gpu.base_color = {{
                static_cast<float>(pbr.baseColorFactor[0]),
                static_cast<float>(pbr.baseColorFactor[1]),
                static_cast<float>(pbr.baseColorFactor[2]),
                static_cast<float>(pbr.baseColorFactor[3])
            }};

            gpu.metallic = static_cast<float>(pbr.metallicFactor);
            gpu.roughness = static_cast<float>(pbr.roughnessFactor);

            // Base color texture
            if (pbr.baseColorTexture.index >= 0) {
                gpu.tex_base_color = pbr.baseColorTexture.index;
            }

            // Metallic-roughness texture
            if (pbr.metallicRoughnessTexture.index >= 0) {
                gpu.tex_metallic_roughness = pbr.metallicRoughnessTexture.index;
            }

            // Normal map
            if (mat.normalTexture.index >= 0) {
                gpu.tex_normal = mat.normalTexture.index;
                gpu.set_flag(GpuMaterial::FLAG_HAS_NORMAL_MAP);
            }

            // Occlusion
            if (mat.occlusionTexture.index >= 0) {
                gpu.tex_occlusion = mat.occlusionTexture.index;
            }

            // Emissive
            gpu.emissive = {{
                static_cast<float>(mat.emissiveFactor[0]),
                static_cast<float>(mat.emissiveFactor[1]),
                static_cast<float>(mat.emissiveFactor[2])
            }};

            if (mat.emissiveTexture.index >= 0) {
                gpu.tex_emissive = mat.emissiveTexture.index;
            }

            // Alpha mode
            if (mat.alphaMode == "MASK") {
                gpu.alpha_cutoff = static_cast<float>(mat.alphaCutoff);
                gpu.set_flag(GpuMaterial::FLAG_ALPHA_MASK);
            } else if (mat.alphaMode == "BLEND") {
                gpu.set_flag(GpuMaterial::FLAG_ALPHA_BLEND);
            }

            // Double-sided
            if (mat.doubleSided) {
                gpu.set_flag(GpuMaterial::FLAG_DOUBLE_SIDED);
            }

            // Extensions
            load_material_extensions(mat, gpu);

            // Default shadow flags
            gpu.set_flag(GpuMaterial::FLAG_RECEIVES_SHADOWS);
            gpu.set_flag(GpuMaterial::FLAG_CASTS_SHADOWS);

            scene.materials.push_back(std::move(gltf_mat));
        }

        // Add default material if none loaded
        if (scene.materials.empty()) {
            GltfMaterial default_mat;
            default_mat.name = "default";
            default_mat.gpu_material = GpuMaterial::pbr_default();
            scene.materials.push_back(std::move(default_mat));
        }
    }

    void load_material_extensions(const tinygltf::Material& mat, GpuMaterial& gpu) {
        // KHR_materials_clearcoat
        auto clearcoat_it = mat.extensions.find("KHR_materials_clearcoat");
        if (clearcoat_it != mat.extensions.end()) {
            const auto& ext = clearcoat_it->second;
            if (ext.Has("clearcoatFactor")) {
                gpu.clearcoat = static_cast<float>(ext.Get("clearcoatFactor").GetNumberAsDouble());
            }
            if (ext.Has("clearcoatRoughnessFactor")) {
                gpu.clearcoat_roughness = static_cast<float>(ext.Get("clearcoatRoughnessFactor").GetNumberAsDouble());
            }
            if (gpu.clearcoat > 0) {
                gpu.set_flag(GpuMaterial::FLAG_HAS_CLEARCOAT);
            }
        }

        // KHR_materials_transmission
        auto transmission_it = mat.extensions.find("KHR_materials_transmission");
        if (transmission_it != mat.extensions.end()) {
            const auto& ext = transmission_it->second;
            if (ext.Has("transmissionFactor")) {
                gpu.transmission = static_cast<float>(ext.Get("transmissionFactor").GetNumberAsDouble());
            }
            if (gpu.transmission > 0) {
                gpu.set_flag(GpuMaterial::FLAG_HAS_TRANSMISSION);
            }
        }

        // KHR_materials_ior
        auto ior_it = mat.extensions.find("KHR_materials_ior");
        if (ior_it != mat.extensions.end()) {
            const auto& ext = ior_it->second;
            if (ext.Has("ior")) {
                gpu.ior = static_cast<float>(ext.Get("ior").GetNumberAsDouble());
            }
        }

        // KHR_materials_sheen
        auto sheen_it = mat.extensions.find("KHR_materials_sheen");
        if (sheen_it != mat.extensions.end()) {
            const auto& ext = sheen_it->second;
            if (ext.Has("sheenColorFactor")) {
                const auto& arr = ext.Get("sheenColorFactor");
                if (arr.IsArray() && arr.ArrayLen() >= 3) {
                    gpu.sheen_color = {{
                        static_cast<float>(arr.Get(0).GetNumberAsDouble()),
                        static_cast<float>(arr.Get(1).GetNumberAsDouble()),
                        static_cast<float>(arr.Get(2).GetNumberAsDouble())
                    }};
                    gpu.sheen = 1.0f;  // Enable sheen if color is specified
                }
            }
            if (ext.Has("sheenRoughnessFactor")) {
                gpu.sheen_roughness = static_cast<float>(ext.Get("sheenRoughnessFactor").GetNumberAsDouble());
            }
            if (gpu.sheen > 0) {
                gpu.set_flag(GpuMaterial::FLAG_HAS_SHEEN);
            }
        }

        // KHR_materials_unlit
        if (mat.extensions.find("KHR_materials_unlit") != mat.extensions.end()) {
            gpu.set_flag(GpuMaterial::FLAG_UNLIT);
        }
    }

    // -------------------------------------------------------------------------
    // Mesh loading
    // -------------------------------------------------------------------------

    void load_meshes(const tinygltf::Model& model, GltfScene& scene) {
        scene.meshes.reserve(model.meshes.size());

        for (const auto& mesh : model.meshes) {
            GltfMesh gltf_mesh;
            gltf_mesh.name = mesh.name;

            for (const auto& prim : mesh.primitives) {
                if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
                    continue;  // Only support triangles
                }

                GltfPrimitive gltf_prim;
                gltf_prim.material_index = prim.material;
                gltf_prim.mesh_data.set_topology(PrimitiveTopology::TriangleList);

                // Load vertices
                load_primitive_vertices(model, prim, gltf_prim);

                // Load indices
                load_primitive_indices(model, prim, gltf_prim);

                // Generate tangents if needed
                if (m_options.generate_tangents) {
                    generate_tangents(gltf_prim.mesh_data);
                }

                // Apply scale
                if (m_options.scale != 1.0f) {
                    for (auto& v : gltf_prim.mesh_data.vertices()) {
                        v.position[0] *= m_options.scale;
                        v.position[1] *= m_options.scale;
                        v.position[2] *= m_options.scale;
                    }
                }

                // Update statistics
                scene.total_vertices += gltf_prim.mesh_data.vertex_count();
                scene.total_triangles += gltf_prim.mesh_data.triangle_count();
                scene.total_draw_calls++;

                gltf_mesh.primitives.push_back(std::move(gltf_prim));
            }

            scene.meshes.push_back(std::move(gltf_mesh));
        }
    }

    void load_primitive_vertices(
        const tinygltf::Model& model,
        const tinygltf::Primitive& prim,
        GltfPrimitive& gltf_prim) {

        // Get accessors for each attribute
        const float* positions = nullptr;
        const float* normals = nullptr;
        const float* tangents = nullptr;
        const float* texcoords0 = nullptr;
        const float* texcoords1 = nullptr;
        const float* colors = nullptr;

        std::size_t vertex_count = 0;
        int color_components = 0;

        // Position (required)
        auto pos_it = prim.attributes.find("POSITION");
        if (pos_it != prim.attributes.end()) {
            const auto& accessor = model.accessors[pos_it->second];
            const auto& view = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[view.buffer];
            positions = reinterpret_cast<const float*>(
                buffer.data.data() + view.byteOffset + accessor.byteOffset);
            vertex_count = accessor.count;

            // Bounds
            if (accessor.minValues.size() >= 3) {
                gltf_prim.min_bounds = {{
                    static_cast<float>(accessor.minValues[0]),
                    static_cast<float>(accessor.minValues[1]),
                    static_cast<float>(accessor.minValues[2])
                }};
            }
            if (accessor.maxValues.size() >= 3) {
                gltf_prim.max_bounds = {{
                    static_cast<float>(accessor.maxValues[0]),
                    static_cast<float>(accessor.maxValues[1]),
                    static_cast<float>(accessor.maxValues[2])
                }};
            }
        }

        if (!positions || vertex_count == 0) return;

        // Normal
        auto norm_it = prim.attributes.find("NORMAL");
        if (norm_it != prim.attributes.end()) {
            const auto& accessor = model.accessors[norm_it->second];
            const auto& view = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[view.buffer];
            normals = reinterpret_cast<const float*>(
                buffer.data.data() + view.byteOffset + accessor.byteOffset);
        }

        // Tangent
        auto tan_it = prim.attributes.find("TANGENT");
        if (tan_it != prim.attributes.end()) {
            const auto& accessor = model.accessors[tan_it->second];
            const auto& view = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[view.buffer];
            tangents = reinterpret_cast<const float*>(
                buffer.data.data() + view.byteOffset + accessor.byteOffset);
        }

        // Texcoord 0
        auto uv0_it = prim.attributes.find("TEXCOORD_0");
        if (uv0_it != prim.attributes.end()) {
            const auto& accessor = model.accessors[uv0_it->second];
            const auto& view = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[view.buffer];
            texcoords0 = reinterpret_cast<const float*>(
                buffer.data.data() + view.byteOffset + accessor.byteOffset);
        }

        // Texcoord 1
        auto uv1_it = prim.attributes.find("TEXCOORD_1");
        if (uv1_it != prim.attributes.end()) {
            const auto& accessor = model.accessors[uv1_it->second];
            const auto& view = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[view.buffer];
            texcoords1 = reinterpret_cast<const float*>(
                buffer.data.data() + view.byteOffset + accessor.byteOffset);
        }

        // Vertex colors
        auto color_it = prim.attributes.find("COLOR_0");
        if (color_it != prim.attributes.end()) {
            const auto& accessor = model.accessors[color_it->second];
            const auto& view = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[view.buffer];
            colors = reinterpret_cast<const float*>(
                buffer.data.data() + view.byteOffset + accessor.byteOffset);
            color_components = accessor.type == TINYGLTF_TYPE_VEC4 ? 4 : 3;
        }

        // Build vertices
        gltf_prim.mesh_data.reserve_vertices(vertex_count);

        for (std::size_t i = 0; i < vertex_count; ++i) {
            Vertex v;

            // Position
            v.position = {{positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]}};

            // Normal
            if (normals) {
                v.normal = {{normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]}};
            }

            // Tangent
            if (tangents) {
                v.tangent = {{tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2], tangents[i * 4 + 3]}};
            }

            // UV0
            if (texcoords0) {
                float u = texcoords0[i * 2];
                float vv = texcoords0[i * 2 + 1];
                if (m_options.flip_uvs) vv = 1.0f - vv;
                v.uv0 = {{u, vv}};
            }

            // UV1
            if (texcoords1) {
                float u = texcoords1[i * 2];
                float vv = texcoords1[i * 2 + 1];
                if (m_options.flip_uvs) vv = 1.0f - vv;
                v.uv1 = {{u, vv}};
            } else {
                v.uv1 = v.uv0;
            }

            // Color
            if (colors) {
                v.color[0] = colors[i * color_components];
                v.color[1] = colors[i * color_components + 1];
                v.color[2] = colors[i * color_components + 2];
                v.color[3] = color_components == 4 ? colors[i * color_components + 3] : 1.0f;
            }

            gltf_prim.mesh_data.add_vertex(v);
        }
    }

    void load_primitive_indices(
        const tinygltf::Model& model,
        const tinygltf::Primitive& prim,
        GltfPrimitive& gltf_prim) {

        if (prim.indices < 0) return;

        const auto& accessor = model.accessors[prim.indices];
        const auto& view = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[view.buffer];

        const std::uint8_t* data = buffer.data.data() + view.byteOffset + accessor.byteOffset;

        gltf_prim.mesh_data.reserve_indices(accessor.count);

        switch (accessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                for (std::size_t i = 0; i < accessor.count; ++i) {
                    gltf_prim.mesh_data.add_index(static_cast<std::uint32_t>(data[i]));
                }
                break;

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const auto* indices16 = reinterpret_cast<const std::uint16_t*>(data);
                for (std::size_t i = 0; i < accessor.count; ++i) {
                    gltf_prim.mesh_data.add_index(static_cast<std::uint32_t>(indices16[i]));
                }
                break;
            }

            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                const auto* indices32 = reinterpret_cast<const std::uint32_t*>(data);
                for (std::size_t i = 0; i < accessor.count; ++i) {
                    gltf_prim.mesh_data.add_index(indices32[i]);
                }
                break;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Tangent generation (MikkTSpace-lite)
    // -------------------------------------------------------------------------

    void generate_tangents(MeshData& mesh) {
        auto& vertices = mesh.vertices();
        const auto& indices = mesh.indices();

        if (vertices.empty()) return;

        std::size_t tri_count = mesh.is_indexed() ? indices.size() / 3 : vertices.size() / 3;

        // Accumulate tangents
        std::vector<std::array<float, 3>> tan1(vertices.size(), {{0, 0, 0}});
        std::vector<std::array<float, 3>> tan2(vertices.size(), {{0, 0, 0}});

        for (std::size_t t = 0; t < tri_count; ++t) {
            std::size_t i0, i1, i2;
            if (mesh.is_indexed()) {
                i0 = indices[t * 3];
                i1 = indices[t * 3 + 1];
                i2 = indices[t * 3 + 2];
            } else {
                i0 = t * 3;
                i1 = t * 3 + 1;
                i2 = t * 3 + 2;
            }

            const Vertex& v0 = vertices[i0];
            const Vertex& v1 = vertices[i1];
            const Vertex& v2 = vertices[i2];

            float x1 = v1.position[0] - v0.position[0];
            float x2 = v2.position[0] - v0.position[0];
            float y1 = v1.position[1] - v0.position[1];
            float y2 = v2.position[1] - v0.position[1];
            float z1 = v1.position[2] - v0.position[2];
            float z2 = v2.position[2] - v0.position[2];

            float s1 = v1.uv0[0] - v0.uv0[0];
            float s2 = v2.uv0[0] - v0.uv0[0];
            float t1 = v1.uv0[1] - v0.uv0[1];
            float t2 = v2.uv0[1] - v0.uv0[1];

            float denom = s1 * t2 - s2 * t1;
            float r = denom != 0.0f ? 1.0f / denom : 0.0f;

            std::array<float, 3> sdir = {{
                (t2 * x1 - t1 * x2) * r,
                (t2 * y1 - t1 * y2) * r,
                (t2 * z1 - t1 * z2) * r
            }};

            std::array<float, 3> tdir = {{
                (s1 * x2 - s2 * x1) * r,
                (s1 * y2 - s2 * y1) * r,
                (s1 * z2 - s2 * z1) * r
            }};

            for (std::size_t idx : {i0, i1, i2}) {
                tan1[idx][0] += sdir[0];
                tan1[idx][1] += sdir[1];
                tan1[idx][2] += sdir[2];
                tan2[idx][0] += tdir[0];
                tan2[idx][1] += tdir[1];
                tan2[idx][2] += tdir[2];
            }
        }

        // Orthonormalize and write tangents
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto& n = vertices[i].normal;
            const auto& t = tan1[i];

            // Gram-Schmidt orthogonalize
            float dot = n[0] * t[0] + n[1] * t[1] + n[2] * t[2];
            std::array<float, 3> tangent = {{
                t[0] - n[0] * dot,
                t[1] - n[1] * dot,
                t[2] - n[2] * dot
            }};

            // Normalize
            float len = std::sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1] + tangent[2] * tangent[2]);
            if (len > 1e-6f) {
                tangent[0] /= len;
                tangent[1] /= len;
                tangent[2] /= len;
            } else {
                tangent = {{1, 0, 0}};
            }

            // Handedness
            float cross_x = n[1] * tangent[2] - n[2] * tangent[1];
            float cross_y = n[2] * tangent[0] - n[0] * tangent[2];
            float cross_z = n[0] * tangent[1] - n[1] * tangent[0];
            float w = (cross_x * tan2[i][0] + cross_y * tan2[i][1] + cross_z * tan2[i][2]) < 0.0f ? -1.0f : 1.0f;

            vertices[i].tangent = {{tangent[0], tangent[1], tangent[2], w}};
        }
    }

    // -------------------------------------------------------------------------
    // Node loading
    // -------------------------------------------------------------------------

    void load_nodes(const tinygltf::Model& model, GltfScene& scene) {
        scene.nodes.reserve(model.nodes.size());

        for (const auto& node : model.nodes) {
            GltfNode gltf_node;
            gltf_node.name = node.name;
            gltf_node.mesh_index = node.mesh;
            gltf_node.skin_index = node.skin;
            gltf_node.camera_index = node.camera;

            // Transform
            if (!node.matrix.empty()) {
                // Decompose matrix to TRS (simplified - assumes valid TRS matrix)
                gltf_node.local_transform.translation = {{
                    static_cast<float>(node.matrix[12]),
                    static_cast<float>(node.matrix[13]),
                    static_cast<float>(node.matrix[14])
                }};
                // Would decompose rotation/scale properly in production
            } else {
                if (!node.translation.empty()) {
                    gltf_node.local_transform.translation = {{
                        static_cast<float>(node.translation[0]),
                        static_cast<float>(node.translation[1]),
                        static_cast<float>(node.translation[2])
                    }};
                }
                if (!node.rotation.empty()) {
                    gltf_node.local_transform.rotation = {{
                        static_cast<float>(node.rotation[0]),
                        static_cast<float>(node.rotation[1]),
                        static_cast<float>(node.rotation[2]),
                        static_cast<float>(node.rotation[3])
                    }};
                }
                if (!node.scale.empty()) {
                    gltf_node.local_transform.scale = {{
                        static_cast<float>(node.scale[0]),
                        static_cast<float>(node.scale[1]),
                        static_cast<float>(node.scale[2])
                    }};
                }
            }

            // Children
            for (int child : node.children) {
                gltf_node.children.push_back(child);
            }

            scene.nodes.push_back(std::move(gltf_node));
        }

        // Set parent indices
        for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
            for (int child : scene.nodes[i].children) {
                if (child >= 0 && child < static_cast<int>(scene.nodes.size())) {
                    scene.nodes[child].parent = static_cast<int>(i);
                }
            }
        }
    }

    void compute_world_transforms(GltfScene& scene, int node_idx, const std::array<float, 16>& parent_matrix) {
        if (node_idx < 0 || node_idx >= static_cast<int>(scene.nodes.size())) return;

        auto& node = scene.nodes[node_idx];
        auto local = node.local_transform.to_matrix();
        node.world_matrix = GltfTransform::multiply(parent_matrix, local);

        for (int child : node.children) {
            compute_world_transforms(scene, child, node.world_matrix);
        }
    }

    void compute_scene_bounds(GltfScene& scene) {
        bool first = true;

        for (const auto& node : scene.nodes) {
            if (node.mesh_index < 0) continue;

            const auto& mesh = scene.meshes[node.mesh_index];
            for (const auto& prim : mesh.primitives) {
                // Transform bounding box corners by world matrix
                std::array<std::array<float, 3>, 8> corners = {{
                    {{prim.min_bounds[0], prim.min_bounds[1], prim.min_bounds[2]}},
                    {{prim.max_bounds[0], prim.min_bounds[1], prim.min_bounds[2]}},
                    {{prim.min_bounds[0], prim.max_bounds[1], prim.min_bounds[2]}},
                    {{prim.max_bounds[0], prim.max_bounds[1], prim.min_bounds[2]}},
                    {{prim.min_bounds[0], prim.min_bounds[1], prim.max_bounds[2]}},
                    {{prim.max_bounds[0], prim.min_bounds[1], prim.max_bounds[2]}},
                    {{prim.min_bounds[0], prim.max_bounds[1], prim.max_bounds[2]}},
                    {{prim.max_bounds[0], prim.max_bounds[1], prim.max_bounds[2]}}
                }};

                for (const auto& corner : corners) {
                    // Transform by world matrix
                    float x = node.world_matrix[0] * corner[0] + node.world_matrix[4] * corner[1] +
                              node.world_matrix[8] * corner[2] + node.world_matrix[12];
                    float y = node.world_matrix[1] * corner[0] + node.world_matrix[5] * corner[1] +
                              node.world_matrix[9] * corner[2] + node.world_matrix[13];
                    float z = node.world_matrix[2] * corner[0] + node.world_matrix[6] * corner[1] +
                              node.world_matrix[10] * corner[2] + node.world_matrix[14];

                    if (first) {
                        scene.min_bounds = {{x, y, z}};
                        scene.max_bounds = {{x, y, z}};
                        first = false;
                    } else {
                        scene.min_bounds[0] = std::min(scene.min_bounds[0], x);
                        scene.min_bounds[1] = std::min(scene.min_bounds[1], y);
                        scene.min_bounds[2] = std::min(scene.min_bounds[2], z);
                        scene.max_bounds[0] = std::max(scene.max_bounds[0], x);
                        scene.max_bounds[1] = std::max(scene.max_bounds[1], y);
                        scene.max_bounds[2] = std::max(scene.max_bounds[2], z);
                    }
                }
            }
        }
    }
};

// =============================================================================
// GltfLoader Public Interface
// =============================================================================

GltfLoader::GltfLoader() : m_impl(std::make_unique<Impl>()) {}
GltfLoader::~GltfLoader() = default;

GltfLoader::GltfLoader(GltfLoader&&) noexcept = default;
GltfLoader& GltfLoader::operator=(GltfLoader&&) noexcept = default;

GltfLoader::LoadOptions GltfLoader::default_options() {
    return LoadOptions{};
}

std::optional<GltfScene> GltfLoader::load(const std::string& path) {
    return m_impl->load(path, default_options());
}

std::optional<GltfScene> GltfLoader::load(const std::string& path, const LoadOptions& options) {
    return m_impl->load(path, options);
}

const std::string& GltfLoader::last_error() const noexcept {
    return m_impl->m_last_error;
}

// =============================================================================
// GltfSceneManager::Impl
// =============================================================================

class GltfSceneManager::Impl {
public:
    std::vector<SceneEntry> m_scenes;
};

// =============================================================================
// GltfSceneManager Public Interface
// =============================================================================

GltfSceneManager::GltfSceneManager() : m_impl(std::make_unique<Impl>()) {}
GltfSceneManager::~GltfSceneManager() = default;

GltfSceneManager::GltfSceneManager(GltfSceneManager&&) noexcept = default;
GltfSceneManager& GltfSceneManager::operator=(GltfSceneManager&&) noexcept = default;

std::optional<std::size_t> GltfSceneManager::load(
    const std::string& path,
    const GltfLoader::LoadOptions& options) {

    GltfLoader loader;
    auto scene = loader.load(path, options);
    if (!scene) {
        return std::nullopt;
    }

    // Check for existing scene
    for (std::size_t i = 0; i < m_impl->m_scenes.size(); ++i) {
        if (m_impl->m_scenes[i].path == path) {
            m_impl->m_scenes[i].scene = std::move(*scene);
            std::error_code ec;
            m_impl->m_scenes[i].last_modified = std::filesystem::last_write_time(path, ec);
            m_impl->m_scenes[i].dirty = false;
            return i;
        }
    }

    // Add new scene
    SceneEntry entry;
    entry.path = path;
    entry.scene = std::move(*scene);
    if (std::filesystem::exists(path)) {
        std::error_code ec;
        entry.last_modified = std::filesystem::last_write_time(path, ec);
    }

    std::size_t index = m_impl->m_scenes.size();
    m_impl->m_scenes.push_back(std::move(entry));
    return index;
}

GltfScene* GltfSceneManager::get(std::size_t index) {
    if (index >= m_impl->m_scenes.size()) return nullptr;
    return &m_impl->m_scenes[index].scene;
}

const GltfScene* GltfSceneManager::get(std::size_t index) const {
    if (index >= m_impl->m_scenes.size()) return nullptr;
    return &m_impl->m_scenes[index].scene;
}

void GltfSceneManager::check_hot_reload(const GltfLoader::LoadOptions& options) {
    for (auto& entry : m_impl->m_scenes) {
        if (!std::filesystem::exists(entry.path)) continue;

        std::error_code ec;
        auto current_time = std::filesystem::last_write_time(entry.path, ec);
        if (ec) continue;

        if (current_time != entry.last_modified) {
            GltfLoader loader;
            if (auto scene = loader.load(entry.path, options)) {
                entry.scene = std::move(*scene);
                entry.last_modified = current_time;
                entry.dirty = true;
                spdlog::info("Hot-reloaded glTF: {}", entry.path);
            }
        }
    }
}

bool GltfSceneManager::is_dirty(std::size_t index) const {
    if (index >= m_impl->m_scenes.size()) return false;
    return m_impl->m_scenes[index].dirty;
}

void GltfSceneManager::clear_dirty(std::size_t index) {
    if (index < m_impl->m_scenes.size()) {
        m_impl->m_scenes[index].dirty = false;
    }
}

std::size_t GltfSceneManager::count() const noexcept {
    return m_impl->m_scenes.size();
}

void GltfSceneManager::remove(std::size_t index) {
    if (index < m_impl->m_scenes.size()) {
        m_impl->m_scenes.erase(m_impl->m_scenes.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void GltfSceneManager::clear() {
    m_impl->m_scenes.clear();
}

// =============================================================================
// Utility Functions
// =============================================================================

bool is_model_path(const std::string& mesh_name) {
    // Check if the string looks like a file path rather than a built-in mesh name
    return mesh_name.find('/') != std::string::npos ||
           mesh_name.find('\\') != std::string::npos ||
           mesh_name.find(".gltf") != std::string::npos ||
           mesh_name.find(".glb") != std::string::npos ||
           mesh_name.find(".obj") != std::string::npos ||
           mesh_name.find(".fbx") != std::string::npos;
}

} // namespace void_render
