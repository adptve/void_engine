/// @file model_loader.cpp
/// @brief 3D model asset loader implementation

#include <void_engine/asset/loaders/model_loader.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

// tinygltf forward declarations (assuming linked)
#ifdef VOID_HAS_TINYGLTF
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#endif

namespace void_asset {

// =============================================================================
// ModelLoader Implementation
// =============================================================================

LoadResult<ModelAsset> ModelLoader::load(LoadContext& ctx) {
    std::string ext = ctx.extension();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "gltf") {
        return load_gltf(ctx, false);
    } else if (ext == "glb") {
        return load_gltf(ctx, true);
    } else if (ext == "obj") {
        return load_obj(ctx);
    }

    return void_core::Err<std::unique_ptr<ModelAsset>>(
        void_core::Error("Unsupported model format: " + ext));
}

#ifdef VOID_HAS_TINYGLTF

LoadResult<ModelAsset> ModelLoader::load_gltf(LoadContext& ctx, bool is_binary) {
    tinygltf::Model gltf_model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    const auto& data = ctx.data();
    bool success = false;

    if (is_binary) {
        success = loader.LoadBinaryFromMemory(
            &gltf_model, &err, &warn,
            data.data(), static_cast<unsigned int>(data.size()));
    } else {
        std::string content(data.begin(), data.end());
        std::string base_dir = ctx.path().parent().str();
        success = loader.LoadASCIIFromString(
            &gltf_model, &err, &warn,
            content.c_str(), static_cast<unsigned int>(content.size()),
            base_dir);
    }

    if (!success) {
        return void_core::Err<std::unique_ptr<ModelAsset>>(
            void_core::Error("Failed to load glTF: " + err));
    }

    auto asset = std::make_unique<ModelAsset>();
    asset->name = ctx.path().filename();
    asset->source_path = ctx.path().str();

    // Load meshes
    for (const auto& gltf_mesh : gltf_model.meshes) {
        ModelMesh mesh;
        mesh.name = gltf_mesh.name;

        for (const auto& gltf_prim : gltf_mesh.primitives) {
            MeshPrimitive prim;
            prim.material_index = gltf_prim.material;

            // Set topology
            switch (gltf_prim.mode) {
                case TINYGLTF_MODE_POINTS: prim.topology = PrimitiveTopology::Points; break;
                case TINYGLTF_MODE_LINE: prim.topology = PrimitiveTopology::Lines; break;
                case TINYGLTF_MODE_LINE_STRIP: prim.topology = PrimitiveTopology::LineStrip; break;
                case TINYGLTF_MODE_TRIANGLES: prim.topology = PrimitiveTopology::Triangles; break;
                case TINYGLTF_MODE_TRIANGLE_STRIP: prim.topology = PrimitiveTopology::TriangleStrip; break;
                case TINYGLTF_MODE_TRIANGLE_FAN: prim.topology = PrimitiveTopology::TriangleFan; break;
                default: prim.topology = PrimitiveTopology::Triangles; break;
            }

            // Helper to extract accessor data
            auto extract_accessor = [&](int accessor_idx, std::vector<float>& out, int components) {
                if (accessor_idx < 0) return;

                const auto& accessor = gltf_model.accessors[accessor_idx];
                const auto& view = gltf_model.bufferViews[accessor.bufferView];
                const auto& buffer = gltf_model.buffers[view.buffer];

                const std::uint8_t* base = buffer.data.data() + view.byteOffset + accessor.byteOffset;
                std::size_t stride = view.byteStride ? view.byteStride : (components * sizeof(float));

                out.resize(accessor.count * components);
                for (std::size_t i = 0; i < accessor.count; ++i) {
                    const float* src = reinterpret_cast<const float*>(base + i * stride);
                    for (int c = 0; c < components; ++c) {
                        out[i * components + c] = src[c];
                    }
                }
            };

            // Extract attributes
            for (const auto& [name, accessor_idx] : gltf_prim.attributes) {
                if (name == "POSITION") {
                    extract_accessor(accessor_idx, prim.positions, 3);
                } else if (name == "NORMAL") {
                    extract_accessor(accessor_idx, prim.normals, 3);
                } else if (name == "TANGENT") {
                    extract_accessor(accessor_idx, prim.tangents, 4);
                } else if (name == "TEXCOORD_0") {
                    extract_accessor(accessor_idx, prim.texcoords0, 2);

                    // Flip UVs if configured
                    if (m_config.flip_uvs) {
                        for (std::size_t i = 1; i < prim.texcoords0.size(); i += 2) {
                            prim.texcoords0[i] = 1.0f - prim.texcoords0[i];
                        }
                    }
                } else if (name == "TEXCOORD_1") {
                    extract_accessor(accessor_idx, prim.texcoords1, 2);
                } else if (name == "COLOR_0") {
                    extract_accessor(accessor_idx, prim.colors0, 4);
                } else if (name == "WEIGHTS_0") {
                    extract_accessor(accessor_idx, prim.weights0, 4);
                } else if (name == "JOINTS_0") {
                    // Joints are typically unsigned bytes
                    const auto& accessor = gltf_model.accessors[accessor_idx];
                    const auto& view = gltf_model.bufferViews[accessor.bufferView];
                    const auto& buffer = gltf_model.buffers[view.buffer];

                    const std::uint8_t* base = buffer.data.data() + view.byteOffset + accessor.byteOffset;
                    prim.joints0.resize(accessor.count * 4);

                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        std::memcpy(prim.joints0.data(), base, accessor.count * 4);
                    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const std::uint16_t* joints = reinterpret_cast<const std::uint16_t*>(base);
                        for (std::size_t i = 0; i < accessor.count * 4; ++i) {
                            prim.joints0[i] = static_cast<std::uint8_t>(joints[i]);
                        }
                    }
                }
            }

            // Extract indices
            if (gltf_prim.indices >= 0) {
                const auto& accessor = gltf_model.accessors[gltf_prim.indices];
                const auto& view = gltf_model.bufferViews[accessor.bufferView];
                const auto& buffer = gltf_model.buffers[view.buffer];

                const std::uint8_t* base = buffer.data.data() + view.byteOffset + accessor.byteOffset;
                prim.indices.resize(accessor.count);

                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const std::uint16_t* indices = reinterpret_cast<const std::uint16_t*>(base);
                    for (std::size_t i = 0; i < accessor.count; ++i) {
                        prim.indices[i] = indices[i];
                    }
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const std::uint32_t* indices = reinterpret_cast<const std::uint32_t*>(base);
                    std::memcpy(prim.indices.data(), indices, accessor.count * sizeof(std::uint32_t));
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const std::uint8_t* indices = base;
                    for (std::size_t i = 0; i < accessor.count; ++i) {
                        prim.indices[i] = indices[i];
                    }
                }
            }

            // Generate tangents if missing and requested
            if (m_config.generate_tangents && prim.tangents.empty() &&
                !prim.normals.empty() && !prim.texcoords0.empty()) {
                generate_tangents(prim);
            }

            mesh.primitives.push_back(std::move(prim));
        }

        asset->meshes.push_back(std::move(mesh));
    }

    // Load materials
    for (const auto& gltf_mat : gltf_model.materials) {
        ModelMaterial mat;
        mat.name = gltf_mat.name;

        // PBR metallic-roughness
        const auto& pbr = gltf_mat.pbrMetallicRoughness;
        mat.base_color_factor = {
            static_cast<float>(pbr.baseColorFactor[0]),
            static_cast<float>(pbr.baseColorFactor[1]),
            static_cast<float>(pbr.baseColorFactor[2]),
            static_cast<float>(pbr.baseColorFactor[3])
        };
        mat.metallic_factor = static_cast<float>(pbr.metallicFactor);
        mat.roughness_factor = static_cast<float>(pbr.roughnessFactor);

        if (pbr.baseColorTexture.index >= 0) {
            mat.base_color_texture = pbr.baseColorTexture.index;
        }
        if (pbr.metallicRoughnessTexture.index >= 0) {
            mat.metallic_roughness_texture = pbr.metallicRoughnessTexture.index;
        }

        // Normal
        if (gltf_mat.normalTexture.index >= 0) {
            mat.normal_texture = gltf_mat.normalTexture.index;
            mat.normal_scale = static_cast<float>(gltf_mat.normalTexture.scale);
        }

        // Occlusion
        if (gltf_mat.occlusionTexture.index >= 0) {
            mat.occlusion_texture = gltf_mat.occlusionTexture.index;
            mat.occlusion_strength = static_cast<float>(gltf_mat.occlusionTexture.strength);
        }

        // Emissive
        if (gltf_mat.emissiveTexture.index >= 0) {
            mat.emissive_texture = gltf_mat.emissiveTexture.index;
        }
        mat.emissive_factor = {
            static_cast<float>(gltf_mat.emissiveFactor[0]),
            static_cast<float>(gltf_mat.emissiveFactor[1]),
            static_cast<float>(gltf_mat.emissiveFactor[2])
        };

        // Alpha
        mat.alpha_cutoff = static_cast<float>(gltf_mat.alphaCutoff);
        mat.double_sided = gltf_mat.doubleSided;
        if (gltf_mat.alphaMode == "OPAQUE") {
            mat.alpha_mode = ModelMaterial::AlphaMode::Opaque;
        } else if (gltf_mat.alphaMode == "MASK") {
            mat.alpha_mode = ModelMaterial::AlphaMode::Mask;
        } else if (gltf_mat.alphaMode == "BLEND") {
            mat.alpha_mode = ModelMaterial::AlphaMode::Blend;
        }

        asset->materials.push_back(std::move(mat));
    }

    // Load textures
    for (const auto& gltf_tex : gltf_model.textures) {
        ModelTexture tex;
        tex.name = gltf_tex.name;
        tex.sampler_index = gltf_tex.sampler;

        if (gltf_tex.source >= 0) {
            const auto& img = gltf_model.images[gltf_tex.source];
            tex.uri = img.uri;

            // Store embedded texture data if present
            if (!img.image.empty()) {
                tex.embedded_data = img.image;
            }
        }

        asset->textures.push_back(std::move(tex));
    }

    // Load samplers
    for (const auto& gltf_sampler : gltf_model.samplers) {
        ModelSampler sampler;

        switch (gltf_sampler.magFilter) {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                sampler.mag_filter = ModelSampler::Filter::Nearest; break;
            default:
                sampler.mag_filter = ModelSampler::Filter::Linear; break;
        }

        switch (gltf_sampler.minFilter) {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                sampler.min_filter = ModelSampler::Filter::Nearest; break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                sampler.min_filter = ModelSampler::Filter::NearestMipmapNearest; break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                sampler.min_filter = ModelSampler::Filter::LinearMipmapNearest; break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                sampler.min_filter = ModelSampler::Filter::NearestMipmapLinear; break;
            default:
                sampler.min_filter = ModelSampler::Filter::LinearMipmapLinear; break;
        }

        switch (gltf_sampler.wrapS) {
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                sampler.wrap_s = ModelSampler::Wrap::ClampToEdge; break;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                sampler.wrap_s = ModelSampler::Wrap::MirroredRepeat; break;
            default:
                sampler.wrap_s = ModelSampler::Wrap::Repeat; break;
        }

        switch (gltf_sampler.wrapT) {
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                sampler.wrap_t = ModelSampler::Wrap::ClampToEdge; break;
            case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                sampler.wrap_t = ModelSampler::Wrap::MirroredRepeat; break;
            default:
                sampler.wrap_t = ModelSampler::Wrap::Repeat; break;
        }

        asset->samplers.push_back(sampler);
    }

    // Load nodes
    for (const auto& gltf_node : gltf_model.nodes) {
        ModelNode node;
        node.name = gltf_node.name;
        node.mesh_index = gltf_node.mesh;
        node.skin_index = gltf_node.skin;

        if (!gltf_node.translation.empty()) {
            node.translation = {
                static_cast<float>(gltf_node.translation[0]),
                static_cast<float>(gltf_node.translation[1]),
                static_cast<float>(gltf_node.translation[2])
            };
        }

        if (!gltf_node.rotation.empty()) {
            node.rotation = {
                static_cast<float>(gltf_node.rotation[0]),
                static_cast<float>(gltf_node.rotation[1]),
                static_cast<float>(gltf_node.rotation[2]),
                static_cast<float>(gltf_node.rotation[3])
            };
        }

        if (!gltf_node.scale.empty()) {
            node.scale = {
                static_cast<float>(gltf_node.scale[0]),
                static_cast<float>(gltf_node.scale[1]),
                static_cast<float>(gltf_node.scale[2])
            };
        }

        for (int child : gltf_node.children) {
            node.children.push_back(static_cast<std::uint32_t>(child));
        }

        asset->nodes.push_back(std::move(node));
    }

    // Load skins
    for (const auto& gltf_skin : gltf_model.skins) {
        ModelSkin skin;
        skin.name = gltf_skin.name;
        skin.skeleton_root = gltf_skin.skeleton;

        for (int joint : gltf_skin.joints) {
            skin.joints.push_back(static_cast<std::uint32_t>(joint));
        }

        // Load inverse bind matrices
        if (gltf_skin.inverseBindMatrices >= 0) {
            const auto& accessor = gltf_model.accessors[gltf_skin.inverseBindMatrices];
            const auto& view = gltf_model.bufferViews[accessor.bufferView];
            const auto& buffer = gltf_model.buffers[view.buffer];

            const float* matrices = reinterpret_cast<const float*>(
                buffer.data.data() + view.byteOffset + accessor.byteOffset);

            skin.inverse_bind_matrices.resize(accessor.count);
            for (std::size_t i = 0; i < accessor.count; ++i) {
                std::memcpy(skin.inverse_bind_matrices[i].data(),
                           matrices + i * 16, 16 * sizeof(float));
            }
        }

        asset->skins.push_back(std::move(skin));
    }

    // Load animations
    for (const auto& gltf_anim : gltf_model.animations) {
        ModelAnimation anim;
        anim.name = gltf_anim.name;

        for (const auto& gltf_sampler : gltf_anim.samplers) {
            AnimationSampler sampler;

            // Input (times)
            {
                const auto& accessor = gltf_model.accessors[gltf_sampler.input];
                const auto& view = gltf_model.bufferViews[accessor.bufferView];
                const auto& buffer = gltf_model.buffers[view.buffer];

                const float* data = reinterpret_cast<const float*>(
                    buffer.data.data() + view.byteOffset + accessor.byteOffset);
                sampler.input.assign(data, data + accessor.count);

                // Track max time for duration
                if (!sampler.input.empty() && sampler.input.back() > anim.duration) {
                    anim.duration = sampler.input.back();
                }
            }

            // Output (values)
            {
                const auto& accessor = gltf_model.accessors[gltf_sampler.output];
                const auto& view = gltf_model.bufferViews[accessor.bufferView];
                const auto& buffer = gltf_model.buffers[view.buffer];

                int components = 3;
                if (accessor.type == TINYGLTF_TYPE_VEC4) components = 4;
                else if (accessor.type == TINYGLTF_TYPE_SCALAR) components = 1;

                const float* data = reinterpret_cast<const float*>(
                    buffer.data.data() + view.byteOffset + accessor.byteOffset);
                sampler.output.assign(data, data + accessor.count * components);
            }

            if (gltf_sampler.interpolation == "STEP") {
                sampler.interpolation = AnimationSampler::Interpolation::Step;
            } else if (gltf_sampler.interpolation == "CUBICSPLINE") {
                sampler.interpolation = AnimationSampler::Interpolation::CubicSpline;
            } else {
                sampler.interpolation = AnimationSampler::Interpolation::Linear;
            }

            anim.samplers.push_back(std::move(sampler));
        }

        for (const auto& gltf_channel : gltf_anim.channels) {
            AnimationChannel channel;
            channel.sampler_index = static_cast<std::uint32_t>(gltf_channel.sampler);
            channel.target.node_index = static_cast<std::uint32_t>(gltf_channel.target_node);

            if (gltf_channel.target_path == "translation") {
                channel.target.path = AnimationTarget::Path::Translation;
            } else if (gltf_channel.target_path == "rotation") {
                channel.target.path = AnimationTarget::Path::Rotation;
            } else if (gltf_channel.target_path == "scale") {
                channel.target.path = AnimationTarget::Path::Scale;
            } else if (gltf_channel.target_path == "weights") {
                channel.target.path = AnimationTarget::Path::Weights;
            }

            anim.channels.push_back(channel);
        }

        asset->animations.push_back(std::move(anim));
    }

    // Load scenes
    for (const auto& gltf_scene : gltf_model.scenes) {
        ModelScene scene;
        scene.name = gltf_scene.name;
        for (int node : gltf_scene.nodes) {
            scene.root_nodes.push_back(static_cast<std::uint32_t>(node));
        }
        asset->scenes.push_back(std::move(scene));
    }

    asset->default_scene = gltf_model.defaultScene;

    // Apply scale if configured
    if (std::abs(m_config.scale - 1.0f) > 0.0001f) {
        apply_scale(*asset, m_config.scale);
    }

    return void_core::Ok(std::move(asset));
}

#else // !VOID_HAS_TINYGLTF

LoadResult<ModelAsset> ModelLoader::load_gltf(LoadContext& ctx, bool) {
    (void)ctx;
    return void_core::Err<std::unique_ptr<ModelAsset>>(
        void_core::Error("glTF loading requires tinygltf (VOID_HAS_TINYGLTF not defined)"));
}

#endif // VOID_HAS_TINYGLTF

LoadResult<ModelAsset> ModelLoader::load_obj(LoadContext& ctx) {
    // Simple OBJ parser
    auto asset = std::make_unique<ModelAsset>();
    asset->name = ctx.path().filename();
    asset->source_path = ctx.path().str();

    std::string content = ctx.data_as_string();
    std::istringstream stream(content);
    std::string line;

    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoords;

    ModelMesh mesh;
    mesh.name = "mesh";
    MeshPrimitive prim;
    prim.topology = PrimitiveTopology::Triangles;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream line_stream(line);
        std::string prefix;
        line_stream >> prefix;

        if (prefix == "v") {
            float x, y, z;
            line_stream >> x >> y >> z;
            positions.push_back(x);
            positions.push_back(y);
            positions.push_back(z);
        } else if (prefix == "vn") {
            float x, y, z;
            line_stream >> x >> y >> z;
            normals.push_back(x);
            normals.push_back(y);
            normals.push_back(z);
        } else if (prefix == "vt") {
            float u, v;
            line_stream >> u >> v;
            texcoords.push_back(u);
            texcoords.push_back(m_config.flip_uvs ? (1.0f - v) : v);
        } else if (prefix == "f") {
            // Parse face (triangle or quad)
            std::vector<std::array<int, 3>> face_verts;
            std::string vert;

            while (line_stream >> vert) {
                std::array<int, 3> indices = {-1, -1, -1};
                std::replace(vert.begin(), vert.end(), '/', ' ');
                std::istringstream vert_stream(vert);
                vert_stream >> indices[0];
                if (vert_stream.peek() != EOF) vert_stream >> indices[1];
                if (vert_stream.peek() != EOF) vert_stream >> indices[2];

                face_verts.push_back(indices);
            }

            // Triangulate face
            for (std::size_t i = 1; i + 1 < face_verts.size(); ++i) {
                for (int j : {0, static_cast<int>(i), static_cast<int>(i + 1)}) {
                    const auto& v = face_verts[j];
                    int vi = v[0] - 1;
                    int ti = v[1] - 1;
                    int ni = v[2] - 1;

                    if (vi >= 0 && vi * 3 + 2 < static_cast<int>(positions.size())) {
                        prim.positions.push_back(positions[vi * 3 + 0]);
                        prim.positions.push_back(positions[vi * 3 + 1]);
                        prim.positions.push_back(positions[vi * 3 + 2]);
                    }

                    if (ti >= 0 && ti * 2 + 1 < static_cast<int>(texcoords.size())) {
                        prim.texcoords0.push_back(texcoords[ti * 2 + 0]);
                        prim.texcoords0.push_back(texcoords[ti * 2 + 1]);
                    }

                    if (ni >= 0 && ni * 3 + 2 < static_cast<int>(normals.size())) {
                        prim.normals.push_back(normals[ni * 3 + 0]);
                        prim.normals.push_back(normals[ni * 3 + 1]);
                        prim.normals.push_back(normals[ni * 3 + 2]);
                    }
                }
            }
        }
    }

    // Generate indices (currently non-indexed)
    for (std::uint32_t i = 0; i < prim.vertex_count(); ++i) {
        prim.indices.push_back(i);
    }

    // Generate tangents if needed
    if (m_config.generate_tangents && !prim.normals.empty() && !prim.texcoords0.empty()) {
        generate_tangents(prim);
    }

    mesh.primitives.push_back(std::move(prim));
    asset->meshes.push_back(std::move(mesh));

    // Create default node and scene
    ModelNode node;
    node.name = "root";
    node.mesh_index = 0;
    asset->nodes.push_back(node);

    ModelScene scene;
    scene.name = "default";
    scene.root_nodes.push_back(0);
    asset->scenes.push_back(scene);

    return void_core::Ok(std::move(asset));
}

void ModelLoader::generate_tangents(MeshPrimitive& prim) {
    std::uint32_t vertex_count = prim.vertex_count();
    if (vertex_count == 0) return;

    prim.tangents.resize(vertex_count * 4, 0.0f);

    std::vector<float> tan1(vertex_count * 3, 0.0f);
    std::vector<float> tan2(vertex_count * 3, 0.0f);

    // Compute tangents per triangle
    for (std::size_t i = 0; i + 2 < prim.indices.size(); i += 3) {
        std::uint32_t i0 = prim.indices[i + 0];
        std::uint32_t i1 = prim.indices[i + 1];
        std::uint32_t i2 = prim.indices[i + 2];

        float v0[3] = {prim.positions[i0*3], prim.positions[i0*3+1], prim.positions[i0*3+2]};
        float v1[3] = {prim.positions[i1*3], prim.positions[i1*3+1], prim.positions[i1*3+2]};
        float v2[3] = {prim.positions[i2*3], prim.positions[i2*3+1], prim.positions[i2*3+2]};

        float w0[2] = {prim.texcoords0[i0*2], prim.texcoords0[i0*2+1]};
        float w1[2] = {prim.texcoords0[i1*2], prim.texcoords0[i1*2+1]};
        float w2[2] = {prim.texcoords0[i2*2], prim.texcoords0[i2*2+1]};

        float e1[3] = {v1[0]-v0[0], v1[1]-v0[1], v1[2]-v0[2]};
        float e2[3] = {v2[0]-v0[0], v2[1]-v0[1], v2[2]-v0[2]};

        float du1 = w1[0] - w0[0], dv1 = w1[1] - w0[1];
        float du2 = w2[0] - w0[0], dv2 = w2[1] - w0[1];

        float r = 1.0f / (du1 * dv2 - du2 * dv1 + 1e-10f);

        float sdir[3] = {
            (dv2 * e1[0] - dv1 * e2[0]) * r,
            (dv2 * e1[1] - dv1 * e2[1]) * r,
            (dv2 * e1[2] - dv1 * e2[2]) * r
        };

        float tdir[3] = {
            (du1 * e2[0] - du2 * e1[0]) * r,
            (du1 * e2[1] - du2 * e1[1]) * r,
            (du1 * e2[2] - du2 * e1[2]) * r
        };

        for (auto idx : {i0, i1, i2}) {
            tan1[idx*3+0] += sdir[0];
            tan1[idx*3+1] += sdir[1];
            tan1[idx*3+2] += sdir[2];

            tan2[idx*3+0] += tdir[0];
            tan2[idx*3+1] += tdir[1];
            tan2[idx*3+2] += tdir[2];
        }
    }

    // Orthonormalize and compute handedness
    for (std::uint32_t i = 0; i < vertex_count; ++i) {
        float n[3] = {prim.normals[i*3], prim.normals[i*3+1], prim.normals[i*3+2]};
        float t[3] = {tan1[i*3], tan1[i*3+1], tan1[i*3+2]};

        // Gram-Schmidt orthogonalize
        float dot = n[0]*t[0] + n[1]*t[1] + n[2]*t[2];
        float tangent[3] = {t[0] - n[0]*dot, t[1] - n[1]*dot, t[2] - n[2]*dot};

        // Normalize
        float len = std::sqrt(tangent[0]*tangent[0] + tangent[1]*tangent[1] + tangent[2]*tangent[2]);
        if (len > 1e-6f) {
            tangent[0] /= len; tangent[1] /= len; tangent[2] /= len;
        }

        // Handedness
        float cross[3] = {
            n[1]*t[2] - n[2]*t[1],
            n[2]*t[0] - n[0]*t[2],
            n[0]*t[1] - n[1]*t[0]
        };
        float handedness = (cross[0]*tan2[i*3] + cross[1]*tan2[i*3+1] + cross[2]*tan2[i*3+2]) < 0.0f ? -1.0f : 1.0f;

        prim.tangents[i*4+0] = tangent[0];
        prim.tangents[i*4+1] = tangent[1];
        prim.tangents[i*4+2] = tangent[2];
        prim.tangents[i*4+3] = handedness;
    }
}

void ModelLoader::apply_scale(ModelAsset& model, float scale) {
    for (auto& mesh : model.meshes) {
        for (auto& prim : mesh.primitives) {
            for (std::size_t i = 0; i < prim.positions.size(); ++i) {
                prim.positions[i] *= scale;
            }
        }
    }

    for (auto& node : model.nodes) {
        node.translation[0] *= scale;
        node.translation[1] *= scale;
        node.translation[2] *= scale;
    }
}

} // namespace void_asset
