/// @file vulkan_backend.hpp
/// @brief Vulkan GPU backend implementation
///
/// STATUS: PARTIAL (2026-01-28)
/// - Instance and device creation works
/// - Function pointer loading from vulkan-1.dll / libvulkan.so.1
/// - Buffer and texture creation structure in place
/// - SACRED hot-reload patterns implemented
///
/// TODO to reach PRODUCTION:
/// - [ ] Surface creation (vkCreateWin32SurfaceKHR / vkCreateXlibSurfaceKHR)
/// - [ ] Swapchain (VkSwapchainKHR) integration with GLFW
/// - [ ] Command buffer recording and submission
/// - [ ] Proper memory type selection (query VkPhysicalDeviceMemoryProperties)
/// - [ ] Synchronization (fences, semaphores for frame pacing)
/// - [ ] Pipeline creation with proper VkRenderPass
///
#pragma once

#include "void_engine/render/backend.hpp"
#include <unordered_map>

// Platform detection
#ifdef _WIN32
    #define VOID_PLATFORM_WINDOWS 1
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#elif defined(__linux__)
    #define VOID_PLATFORM_LINUX 1
    #include <dlfcn.h>
#endif

#if defined(VOID_PLATFORM_WINDOWS) || defined(VOID_PLATFORM_LINUX)

namespace void_render {
namespace backends {

// =============================================================================
// Vulkan Type Definitions (avoiding full vulkan.h dependency)
// =============================================================================

#ifndef VK_API_VERSION_1_3
#define VK_API_VERSION_1_3 0x00403000
#define VK_SUCCESS 0
#define VK_INCOMPLETE 5
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 3
#define VK_STRUCTURE_TYPE_SUBMIT_INFO 4
#define VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO 12
#define VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO 14
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 40
#define VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO 39
#define VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 2
#define VK_QUEUE_GRAPHICS_BIT 0x00000001
#define VK_QUEUE_COMPUTE_BIT 0x00000002
#define VK_QUEUE_TRANSFER_BIT 0x00000004
#define VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 0x00000080
#define VK_BUFFER_USAGE_INDEX_BUFFER_BIT 0x00000040
#define VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT 0x00000010
#define VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 0x00000020
#define VK_BUFFER_USAGE_TRANSFER_SRC_BIT 0x00000001
#define VK_BUFFER_USAGE_TRANSFER_DST_BIT 0x00000002
#define VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 0x00000001
#define VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 0x00000002
#define VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 0x00000004
#define VK_FORMAT_R8G8B8A8_UNORM 37
#define VK_FORMAT_B8G8R8A8_UNORM 44
#define VK_FORMAT_R8G8B8A8_SRGB 43
#define VK_FORMAT_D32_SFLOAT 126
#define VK_FORMAT_D24_UNORM_S8_UINT 129
#define VK_IMAGE_TYPE_1D 0
#define VK_IMAGE_TYPE_2D 1
#define VK_IMAGE_TYPE_3D 2
#define VK_IMAGE_TILING_OPTIMAL 0
#define VK_IMAGE_TILING_LINEAR 1
#define VK_IMAGE_USAGE_SAMPLED_BIT 0x00000004
#define VK_IMAGE_USAGE_TRANSFER_DST_BIT 0x00000002
#define VK_COMMAND_BUFFER_LEVEL_PRIMARY 0
#define VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 0x00000002
#endif

// Vulkan handles (opaque pointers)
typedef void* VkInstance;
typedef void* VkPhysicalDevice;
typedef void* VkDevice;
typedef void* VkQueue;
typedef void* VkCommandPool;
typedef void* VkCommandBuffer;
typedef void* VkBuffer;
typedef void* VkDeviceMemory;
typedef void* VkImage;
typedef void* VkImageView;
typedef void* VkSampler;
typedef void* VkShaderModule;
typedef void* VkPipeline;
typedef void* VkPipelineLayout;
typedef void* VkRenderPass;
typedef void* VkFramebuffer;
typedef void* VkDescriptorSetLayout;
typedef void* VkDescriptorPool;
typedef void* VkDescriptorSet;
typedef void* VkFence;
typedef void* VkSemaphore;
typedef std::uint32_t VkFlags;
typedef std::int32_t VkResult;
typedef std::uint64_t VkDeviceSize;

// Vulkan function pointer types
typedef VkResult (*PFN_vkCreateInstance)(const void*, const void*, VkInstance*);
typedef void (*PFN_vkDestroyInstance)(VkInstance, const void*);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance, std::uint32_t*, VkPhysicalDevice*);
typedef void (*PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice, void*);
typedef void (*PFN_vkGetPhysicalDeviceFeatures)(VkPhysicalDevice, void*);
typedef void (*PFN_vkGetPhysicalDeviceMemoryProperties)(VkPhysicalDevice, void*);
typedef void (*PFN_vkGetPhysicalDeviceQueueFamilyProperties)(VkPhysicalDevice, std::uint32_t*, void*);
typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice, const void*, const void*, VkDevice*);
typedef void (*PFN_vkDestroyDevice)(VkDevice, const void*);
typedef void (*PFN_vkGetDeviceQueue)(VkDevice, std::uint32_t, std::uint32_t, VkQueue*);
typedef VkResult (*PFN_vkCreateBuffer)(VkDevice, const void*, const void*, VkBuffer*);
typedef void (*PFN_vkDestroyBuffer)(VkDevice, VkBuffer, const void*);
typedef VkResult (*PFN_vkAllocateMemory)(VkDevice, const void*, const void*, VkDeviceMemory*);
typedef void (*PFN_vkFreeMemory)(VkDevice, VkDeviceMemory, const void*);
typedef VkResult (*PFN_vkMapMemory)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkFlags, void**);
typedef void (*PFN_vkUnmapMemory)(VkDevice, VkDeviceMemory);
typedef VkResult (*PFN_vkBindBufferMemory)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
typedef void (*PFN_vkGetBufferMemoryRequirements)(VkDevice, VkBuffer, void*);
typedef VkResult (*PFN_vkCreateImage)(VkDevice, const void*, const void*, VkImage*);
typedef void (*PFN_vkDestroyImage)(VkDevice, VkImage, const void*);
typedef VkResult (*PFN_vkBindImageMemory)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
typedef void (*PFN_vkGetImageMemoryRequirements)(VkDevice, VkImage, void*);
typedef VkResult (*PFN_vkCreateImageView)(VkDevice, const void*, const void*, VkImageView*);
typedef void (*PFN_vkDestroyImageView)(VkDevice, VkImageView, const void*);
typedef VkResult (*PFN_vkCreateSampler)(VkDevice, const void*, const void*, VkSampler*);
typedef void (*PFN_vkDestroySampler)(VkDevice, VkSampler, const void*);
typedef VkResult (*PFN_vkCreateShaderModule)(VkDevice, const void*, const void*, VkShaderModule*);
typedef void (*PFN_vkDestroyShaderModule)(VkDevice, VkShaderModule, const void*);
typedef VkResult (*PFN_vkCreateGraphicsPipelines)(VkDevice, void*, std::uint32_t, const void*, const void*, VkPipeline*);
typedef VkResult (*PFN_vkCreateComputePipelines)(VkDevice, void*, std::uint32_t, const void*, const void*, VkPipeline*);
typedef void (*PFN_vkDestroyPipeline)(VkDevice, VkPipeline, const void*);
typedef VkResult (*PFN_vkCreateCommandPool)(VkDevice, const void*, const void*, VkCommandPool*);
typedef void (*PFN_vkDestroyCommandPool)(VkDevice, VkCommandPool, const void*);
typedef VkResult (*PFN_vkAllocateCommandBuffers)(VkDevice, const void*, VkCommandBuffer*);
typedef void (*PFN_vkFreeCommandBuffers)(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer*);
typedef VkResult (*PFN_vkBeginCommandBuffer)(VkCommandBuffer, const void*);
typedef VkResult (*PFN_vkEndCommandBuffer)(VkCommandBuffer);
typedef VkResult (*PFN_vkQueueSubmit)(VkQueue, std::uint32_t, const void*, VkFence);
typedef VkResult (*PFN_vkQueueWaitIdle)(VkQueue);
typedef VkResult (*PFN_vkDeviceWaitIdle)(VkDevice);
typedef VkResult (*PFN_vkCreateFence)(VkDevice, const void*, const void*, VkFence*);
typedef void (*PFN_vkDestroyFence)(VkDevice, VkFence, const void*);
typedef VkResult (*PFN_vkWaitForFences)(VkDevice, std::uint32_t, const VkFence*, std::uint32_t, std::uint64_t);
typedef VkResult (*PFN_vkResetFences)(VkDevice, std::uint32_t, const VkFence*);
typedef void* (*PFN_vkGetInstanceProcAddr)(VkInstance, const char*);
typedef void* (*PFN_vkGetDeviceProcAddr)(VkDevice, const char*);

// =============================================================================
// Vulkan Backend Class
// =============================================================================

class VulkanBackend : public gpu::IGpuBackend {
public:
    VulkanBackend() = default;
    ~VulkanBackend() override { shutdown(); }

    // IGpuBackend interface
    gpu::BackendError init(const gpu::BackendConfig& config) override;
    void shutdown() override;

    [[nodiscard]] bool is_initialized() const override { return m_initialized; }
    [[nodiscard]] GpuBackend backend_type() const override { return GpuBackend::Vulkan; }
    [[nodiscard]] const gpu::BackendCapabilities& capabilities() const override { return m_capabilities; }

    gpu::BufferHandle create_buffer(const gpu::BufferDesc& desc) override;
    gpu::TextureHandle create_texture(const gpu::TextureDesc& desc) override;
    gpu::SamplerHandle create_sampler(const gpu::SamplerDesc& desc) override;
    gpu::ShaderModuleHandle create_shader_module(const gpu::ShaderModuleDesc& desc) override;
    gpu::PipelineHandle create_render_pipeline(const gpu::RenderPipelineDesc& desc) override;
    gpu::PipelineHandle create_compute_pipeline(const gpu::ComputePipelineDesc& desc) override;

    void destroy_buffer(gpu::BufferHandle handle) override;
    void destroy_texture(gpu::TextureHandle handle) override;
    void destroy_sampler(gpu::SamplerHandle handle) override;
    void destroy_shader_module(gpu::ShaderModuleHandle handle) override;
    void destroy_pipeline(gpu::PipelineHandle handle) override;

    void write_buffer(gpu::BufferHandle handle, std::size_t offset, const void* data, std::size_t size) override;
    void* map_buffer(gpu::BufferHandle handle, std::size_t offset, std::size_t size) override;
    void unmap_buffer(gpu::BufferHandle handle) override;

    void write_texture(gpu::TextureHandle handle, const void* data, std::size_t size,
                       std::uint32_t mip_level, std::uint32_t array_layer) override;
    void generate_mipmaps(gpu::TextureHandle handle) override;

    gpu::BackendError begin_frame() override;
    gpu::BackendError end_frame() override;
    void present() override;
    void wait_idle() override;

    void resize(std::uint32_t width, std::uint32_t height) override;

    // SACRED hot-reload patterns
    gpu::RehydrationState get_rehydration_state() const override;
    gpu::BackendError rehydrate(const gpu::RehydrationState& state) override;

    gpu::FrameTiming get_frame_timing() const override;
    std::uint64_t get_allocated_memory() const override;

private:
    // Resource tracking structures
    struct VulkanBuffer {
        VkBuffer buffer = nullptr;
        VkDeviceMemory memory = nullptr;
        VkDeviceSize size = 0;
        void* mapped = nullptr;
    };

    struct VulkanTexture {
        VkImage image = nullptr;
        VkImageView view = nullptr;
        VkDeviceMemory memory = nullptr;
        std::uint32_t width = 0, height = 0;
        gpu::TextureFormat format = gpu::TextureFormat::Rgba8Unorm;
    };

    bool m_initialized = false;
    gpu::BackendCapabilities m_capabilities;
    gpu::BackendConfig m_config;
    std::uint64_t m_next_handle = 0;
    std::uint64_t m_frame_number = 0;
    std::uint64_t m_allocated_memory = 0;

    // Vulkan library handle
#ifdef VOID_PLATFORM_WINDOWS
    HMODULE m_vulkan_library = nullptr;
#else
    void* m_vulkan_library = nullptr;
#endif

    // Vulkan core objects
    VkInstance m_instance = nullptr;
    VkPhysicalDevice m_physical_device = nullptr;
    VkDevice m_device = nullptr;
    VkQueue m_graphics_queue = nullptr;
    VkCommandPool m_command_pool = nullptr;
    std::uint32_t m_graphics_queue_family = 0;

    // Resource maps
    std::unordered_map<std::uint64_t, VulkanBuffer> m_buffers;
    std::unordered_map<std::uint64_t, VulkanTexture> m_textures;
    std::unordered_map<std::uint64_t, VkSampler> m_samplers;
    std::unordered_map<std::uint64_t, VkShaderModule> m_shader_modules;
    std::unordered_map<std::uint64_t, VkPipeline> m_pipelines;

    // Vulkan function pointers
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
    PFN_vkCreateInstance vkCreateInstance = nullptr;
    PFN_vkDestroyInstance vkDestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkCreateDevice vkCreateDevice = nullptr;
    PFN_vkDestroyDevice vkDestroyDevice = nullptr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkUnmapMemory vkUnmapMemory = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
    PFN_vkCreateImage vkCreateImage = nullptr;
    PFN_vkDestroyImage vkDestroyImage = nullptr;
    PFN_vkBindImageMemory vkBindImageMemory = nullptr;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
    PFN_vkCreateImageView vkCreateImageView = nullptr;
    PFN_vkDestroyImageView vkDestroyImageView = nullptr;
    PFN_vkCreateSampler vkCreateSampler = nullptr;
    PFN_vkDestroySampler vkDestroySampler = nullptr;
    PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
    PFN_vkCreateComputePipelines vkCreateComputePipelines = nullptr;
    PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;

    // Internal helper methods
    bool load_vulkan_library();
    void unload_vulkan_library();
    bool create_instance(const gpu::BackendConfig& config);
    void destroy_instance();
    bool select_physical_device();
    bool create_device();
    void destroy_device();
    bool create_command_pool();
    void query_capabilities();
    std::uint32_t find_memory_type(VkFlags properties);
    static std::uint32_t texture_format_to_vk(gpu::TextureFormat format);
};

/// Factory function to create Vulkan backend
[[nodiscard]] std::unique_ptr<gpu::IGpuBackend> create_vulkan_backend();

/// Check if Vulkan is available on this system
[[nodiscard]] bool check_vulkan_available();

} // namespace backends
} // namespace void_render

#endif // VOID_PLATFORM_WINDOWS || VOID_PLATFORM_LINUX
