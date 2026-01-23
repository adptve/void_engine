#pragma once

/// @file fwd.hpp
/// @brief Forward declarations for void_shader module

#include <cstdint>

namespace void_shader {

// Shader types
enum class ShaderStage : std::uint8_t;
enum class CompileTarget : std::uint8_t;
enum class BindingType : std::uint8_t;
enum class TextureFormat : std::uint8_t;

// Binding information
struct BindingInfo;
struct BindGroupLayout;
struct VertexInput;
struct FragmentOutput;
struct PushConstantRange;
struct ShaderReflection;

// Source and compilation
struct ShaderSource;
struct ShaderDefine;
struct ShaderVariant;
struct CompiledShader;

// Registry
struct ShaderId;
struct ShaderVersion;
struct ShaderMetadata;
struct ShaderEntry;
class ShaderRegistry;

// Compiler
class ShaderCompiler;
struct CompilerConfig;

// Pipeline
struct ShaderPipelineConfig;
class ShaderPipeline;

// Hot-reload
struct ShaderChangeEvent;
class ShaderWatcher;
class ShaderHotReloadManager;

} // namespace void_shader
