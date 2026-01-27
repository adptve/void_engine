/// @file vulkan_backend.cpp
/// @brief Vulkan GPU backend implementation

#include "vulkan_backend.hpp"
#include <cstring>
#include <vector>
#include <array>

#if defined(VOID_PLATFORM_WINDOWS) || defined(VOID_PLATFORM_LINUX)

namespace void_render {
namespace backends {

using namespace gpu;

// =============================================================================
// Availability Check
// =============================================================================

bool check_vulkan_available() {
#ifdef VOID_PLATFORM_WINDOWS
    HMODULE vulkan = LoadLibraryA("vulkan-1.dll");
    if (vulkan) {
        FreeLibrary(vulkan);
        return true;
    }
    return false;
#elif defined(VOID_PLATFORM_LINUX)
    void* vulkan = dlopen("libvulkan.so.1", RTLD_LAZY);
    if (vulkan) {
        dlclose(vulkan);
        return true;
    }
    return false;
#else
    return false;
#endif
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<IGpuBackend> create_vulkan_backend() {
    return std::make_unique<VulkanBackend>();
}

// =============================================================================
// VulkanBackend Implementation
// =============================================================================

BackendError VulkanBackend::init(const BackendConfig& config) {
    if (m_initialized) return BackendError::AlreadyInitialized;

    // Load Vulkan library
    if (!load_vulkan_library()) {
        return BackendError::UnsupportedBackend;
    }

    // Create Vulkan instance
    if (!create_instance(config)) {
        unload_vulkan_library();
        return BackendError::UnsupportedBackend;
    }

    // Select physical device
    if (!select_physical_device()) {
        destroy_instance();
        unload_vulkan_library();
        return BackendError::UnsupportedBackend;
    }

    // Create logical device
    if (!create_device()) {
        destroy_instance();
        unload_vulkan_library();
        return BackendError::UnsupportedBackend;
    }

    // Create command pool
    if (!create_command_pool()) {
        destroy_device();
        destroy_instance();
        unload_vulkan_library();
        return BackendError::UnsupportedBackend;
    }

    query_capabilities();
    m_config = config;
    m_capabilities.gpu_backend = GpuBackend::Vulkan;
    m_initialized = true;
    return BackendError::None;
}

void VulkanBackend::shutdown() {
    if (!m_initialized) return;

    wait_idle();

    // Destroy all resources
    for (auto& [id, res] : m_buffers) {
        if (vkDestroyBuffer && res.buffer) vkDestroyBuffer(m_device, res.buffer, nullptr);
        if (vkFreeMemory && res.memory) vkFreeMemory(m_device, res.memory, nullptr);
    }
    m_buffers.clear();

    for (auto& [id, res] : m_textures) {
        if (vkDestroyImageView && res.view) vkDestroyImageView(m_device, res.view, nullptr);
        if (vkDestroyImage && res.image) vkDestroyImage(m_device, res.image, nullptr);
        if (vkFreeMemory && res.memory) vkFreeMemory(m_device, res.memory, nullptr);
    }
    m_textures.clear();

    for (auto& [id, sampler] : m_samplers) {
        if (vkDestroySampler && sampler) vkDestroySampler(m_device, sampler, nullptr);
    }
    m_samplers.clear();

    for (auto& [id, pipeline] : m_pipelines) {
        if (vkDestroyPipeline && pipeline) vkDestroyPipeline(m_device, pipeline, nullptr);
    }
    m_pipelines.clear();

    for (auto& [id, module] : m_shader_modules) {
        if (vkDestroyShaderModule && module) vkDestroyShaderModule(m_device, module, nullptr);
    }
    m_shader_modules.clear();

    if (vkDestroyCommandPool && m_command_pool) {
        vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        m_command_pool = nullptr;
    }

    destroy_device();
    destroy_instance();
    unload_vulkan_library();

    m_initialized = false;
}

BufferHandle VulkanBackend::create_buffer(const BufferDesc& desc) {
    if (!m_initialized) return BufferHandle::invalid();

    VulkanBuffer vk_buf;
    vk_buf.size = desc.size;

    // Create buffer
    struct VkBufferCreateInfo {
        std::uint32_t sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        const void* pNext = nullptr;
        VkFlags flags = 0;
        VkDeviceSize size = 0;
        VkFlags usage = 0;
        std::uint32_t sharingMode = 0;
        std::uint32_t queueFamilyIndexCount = 0;
        const std::uint32_t* pQueueFamilyIndices = nullptr;
    } create_info;

    create_info.size = desc.size;
    if (desc.usage & BufferUsage::Vertex) create_info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (desc.usage & BufferUsage::Index) create_info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (desc.usage & BufferUsage::Uniform) create_info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (desc.usage & BufferUsage::Storage) create_info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (!vkCreateBuffer || vkCreateBuffer(m_device, &create_info, nullptr, &vk_buf.buffer) != VK_SUCCESS) {
        return BufferHandle::invalid();
    }

    // Allocate and bind memory
    struct VkMemoryAllocateInfo {
        std::uint32_t sType = 46;  // VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
        const void* pNext = nullptr;
        VkDeviceSize allocationSize = 0;
        std::uint32_t memoryTypeIndex = 0;
    } alloc_info;

    alloc_info.allocationSize = desc.size;
    alloc_info.memoryTypeIndex = find_memory_type(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (!vkAllocateMemory || vkAllocateMemory(m_device, &alloc_info, nullptr, &vk_buf.memory) != VK_SUCCESS) {
        vkDestroyBuffer(m_device, vk_buf.buffer, nullptr);
        return BufferHandle::invalid();
    }

    if (vkBindBufferMemory) {
        vkBindBufferMemory(m_device, vk_buf.buffer, vk_buf.memory, 0);
    }

    m_allocated_memory += desc.size;

    BufferHandle handle{++m_next_handle};
    m_buffers[handle.id] = vk_buf;
    return handle;
}

TextureHandle VulkanBackend::create_texture(const TextureDesc& desc) {
    if (!m_initialized) return TextureHandle::invalid();

    VulkanTexture vk_tex;
    vk_tex.width = desc.width;
    vk_tex.height = desc.height;
    vk_tex.format = desc.format;

    // Create image
    struct VkImageCreateInfo {
        std::uint32_t sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        const void* pNext = nullptr;
        VkFlags flags = 0;
        std::uint32_t imageType = VK_IMAGE_TYPE_2D;
        std::uint32_t format = VK_FORMAT_R8G8B8A8_UNORM;
        std::uint32_t width = 1, height = 1, depth = 1;
        std::uint32_t mipLevels = 1;
        std::uint32_t arrayLayers = 1;
        std::uint32_t samples = 1;
        std::uint32_t tiling = VK_IMAGE_TILING_OPTIMAL;
        VkFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        std::uint32_t sharingMode = 0;
        std::uint32_t queueFamilyIndexCount = 0;
        const std::uint32_t* pQueueFamilyIndices = nullptr;
        std::uint32_t initialLayout = 0;
    } create_info;

    create_info.width = desc.width;
    create_info.height = desc.height;
    create_info.depth = desc.dimension == TextureDimension::D3 ? desc.depth_or_layers : 1;
    create_info.mipLevels = desc.mip_levels;
    create_info.arrayLayers = desc.dimension == TextureDimension::D2Array ? desc.depth_or_layers : 1;
    create_info.format = texture_format_to_vk(desc.format);

    if (!vkCreateImage || vkCreateImage(m_device, &create_info, nullptr, &vk_tex.image) != VK_SUCCESS) {
        return TextureHandle::invalid();
    }

    // Allocate memory
    struct VkMemoryAllocateInfo {
        std::uint32_t sType = 46;
        const void* pNext = nullptr;
        VkDeviceSize allocationSize = 0;
        std::uint32_t memoryTypeIndex = 0;
    } alloc_info;

    alloc_info.allocationSize = desc.width * desc.height * 4;
    alloc_info.memoryTypeIndex = find_memory_type(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory) {
        vkAllocateMemory(m_device, &alloc_info, nullptr, &vk_tex.memory);
        if (vkBindImageMemory && vk_tex.memory) {
            vkBindImageMemory(m_device, vk_tex.image, vk_tex.memory, 0);
        }
    }

    m_allocated_memory += alloc_info.allocationSize;

    TextureHandle handle{++m_next_handle};
    m_textures[handle.id] = vk_tex;
    return handle;
}

SamplerHandle VulkanBackend::create_sampler(const SamplerDesc& desc) {
    if (!m_initialized) return SamplerHandle::invalid();

    VkSampler sampler = nullptr;
    // Real implementation would use VkSamplerCreateInfo

    SamplerHandle handle{++m_next_handle};
    m_samplers[handle.id] = sampler;
    return handle;
}

ShaderModuleHandle VulkanBackend::create_shader_module(const ShaderModuleDesc& desc) {
    if (!m_initialized || desc.spirv.empty()) return ShaderModuleHandle::invalid();

    struct VkShaderModuleCreateInfo {
        std::uint32_t sType = 16;  // VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
        const void* pNext = nullptr;
        VkFlags flags = 0;
        std::size_t codeSize = 0;
        const std::uint32_t* pCode = nullptr;
    } create_info;

    create_info.codeSize = desc.spirv.size() * sizeof(std::uint32_t);
    create_info.pCode = desc.spirv.data();

    VkShaderModule module = nullptr;
    if (!vkCreateShaderModule || vkCreateShaderModule(m_device, &create_info, nullptr, &module) != VK_SUCCESS) {
        return ShaderModuleHandle::invalid();
    }

    ShaderModuleHandle handle{++m_next_handle};
    m_shader_modules[handle.id] = module;
    return handle;
}

PipelineHandle VulkanBackend::create_render_pipeline(const RenderPipelineDesc& desc) {
    if (!m_initialized) return PipelineHandle::invalid();

    // Real implementation would create full graphics pipeline
    VkPipeline pipeline = nullptr;

    PipelineHandle handle{++m_next_handle};
    m_pipelines[handle.id] = pipeline;
    return handle;
}

PipelineHandle VulkanBackend::create_compute_pipeline(const ComputePipelineDesc& desc) {
    if (!m_initialized) return PipelineHandle::invalid();

    VkPipeline pipeline = nullptr;

    PipelineHandle handle{++m_next_handle};
    m_pipelines[handle.id] = pipeline;
    return handle;
}

void VulkanBackend::destroy_buffer(BufferHandle handle) {
    auto it = m_buffers.find(handle.id);
    if (it != m_buffers.end()) {
        if (vkDestroyBuffer && it->second.buffer) vkDestroyBuffer(m_device, it->second.buffer, nullptr);
        if (vkFreeMemory && it->second.memory) vkFreeMemory(m_device, it->second.memory, nullptr);
        m_allocated_memory -= it->second.size;
        m_buffers.erase(it);
    }
}

void VulkanBackend::destroy_texture(TextureHandle handle) {
    auto it = m_textures.find(handle.id);
    if (it != m_textures.end()) {
        if (vkDestroyImageView && it->second.view) vkDestroyImageView(m_device, it->second.view, nullptr);
        if (vkDestroyImage && it->second.image) vkDestroyImage(m_device, it->second.image, nullptr);
        if (vkFreeMemory && it->second.memory) vkFreeMemory(m_device, it->second.memory, nullptr);
        m_textures.erase(it);
    }
}

void VulkanBackend::destroy_sampler(SamplerHandle handle) {
    auto it = m_samplers.find(handle.id);
    if (it != m_samplers.end()) {
        if (vkDestroySampler && it->second) vkDestroySampler(m_device, it->second, nullptr);
        m_samplers.erase(it);
    }
}

void VulkanBackend::destroy_shader_module(ShaderModuleHandle handle) {
    auto it = m_shader_modules.find(handle.id);
    if (it != m_shader_modules.end()) {
        if (vkDestroyShaderModule && it->second) vkDestroyShaderModule(m_device, it->second, nullptr);
        m_shader_modules.erase(it);
    }
}

void VulkanBackend::destroy_pipeline(PipelineHandle handle) {
    auto it = m_pipelines.find(handle.id);
    if (it != m_pipelines.end()) {
        if (vkDestroyPipeline && it->second) vkDestroyPipeline(m_device, it->second, nullptr);
        m_pipelines.erase(it);
    }
}

void VulkanBackend::write_buffer(BufferHandle handle, std::size_t offset, const void* data, std::size_t size) {
    auto it = m_buffers.find(handle.id);
    if (it == m_buffers.end() || !it->second.memory) return;

    void* mapped = nullptr;
    if (vkMapMemory && vkMapMemory(m_device, it->second.memory, offset, size, 0, &mapped) == VK_SUCCESS) {
        std::memcpy(mapped, data, size);
        if (vkUnmapMemory) vkUnmapMemory(m_device, it->second.memory);
    }
}

void* VulkanBackend::map_buffer(BufferHandle handle, std::size_t offset, std::size_t size) {
    auto it = m_buffers.find(handle.id);
    if (it == m_buffers.end() || !it->second.memory) return nullptr;

    void* mapped = nullptr;
    if (vkMapMemory && vkMapMemory(m_device, it->second.memory, offset, size, 0, &mapped) == VK_SUCCESS) {
        it->second.mapped = mapped;
        return mapped;
    }
    return nullptr;
}

void VulkanBackend::unmap_buffer(BufferHandle handle) {
    auto it = m_buffers.find(handle.id);
    if (it != m_buffers.end() && it->second.memory && it->second.mapped) {
        if (vkUnmapMemory) vkUnmapMemory(m_device, it->second.memory);
        it->second.mapped = nullptr;
    }
}

void VulkanBackend::write_texture(TextureHandle handle, const void* data, std::size_t size,
                                   std::uint32_t mip_level, std::uint32_t array_layer) {
    // Real implementation would use staging buffer and vkCmdCopyBufferToImage
    (void)handle; (void)data; (void)size; (void)mip_level; (void)array_layer;
}

void VulkanBackend::generate_mipmaps(TextureHandle handle) {
    // Real implementation would use vkCmdBlitImage
    (void)handle;
}

BackendError VulkanBackend::begin_frame() {
    m_frame_number++;
    return BackendError::None;
}

BackendError VulkanBackend::end_frame() {
    return BackendError::None;
}

void VulkanBackend::present() {
    // Would use VkSwapchain
}

void VulkanBackend::wait_idle() {
    if (vkDeviceWaitIdle && m_device) {
        vkDeviceWaitIdle(m_device);
    }
}

void VulkanBackend::resize(std::uint32_t width, std::uint32_t height) {
    m_config.initial_width = width;
    m_config.initial_height = height;
    // Would recreate swapchain
}

RehydrationState VulkanBackend::get_rehydration_state() const {
    RehydrationState state;
    state.width = m_config.initial_width;
    state.height = m_config.initial_height;
    state.fullscreen = m_config.fullscreen;
    state.vsync = m_config.vsync;
    state.frame_count = m_frame_number;
    return state;
}

BackendError VulkanBackend::rehydrate(const RehydrationState& state) {
    resize(state.width, state.height);
    m_config.fullscreen = state.fullscreen;
    m_config.vsync = state.vsync;
    m_frame_number = state.frame_count;
    return BackendError::None;
}

FrameTiming VulkanBackend::get_frame_timing() const {
    FrameTiming timing;
    timing.frame_number = m_frame_number;
    return timing;
}

std::uint64_t VulkanBackend::get_allocated_memory() const {
    return m_allocated_memory;
}

// =============================================================================
// Internal Helper Methods
// =============================================================================

bool VulkanBackend::load_vulkan_library() {
#ifdef VOID_PLATFORM_WINDOWS
    m_vulkan_library = LoadLibraryA("vulkan-1.dll");
    if (!m_vulkan_library) return false;

    #define LOAD_VK(name) name = (decltype(name))GetProcAddress(m_vulkan_library, #name)
#else
    m_vulkan_library = dlopen("libvulkan.so.1", RTLD_NOW);
    if (!m_vulkan_library) return false;

    #define LOAD_VK(name) name = (decltype(name))dlsym(m_vulkan_library, #name)
#endif

    LOAD_VK(vkGetInstanceProcAddr);
    LOAD_VK(vkCreateInstance);
    LOAD_VK(vkDestroyInstance);
    LOAD_VK(vkEnumeratePhysicalDevices);
    LOAD_VK(vkGetPhysicalDeviceProperties);
    LOAD_VK(vkGetPhysicalDeviceFeatures);
    LOAD_VK(vkGetPhysicalDeviceMemoryProperties);
    LOAD_VK(vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD_VK(vkCreateDevice);
    LOAD_VK(vkDestroyDevice);
    LOAD_VK(vkGetDeviceQueue);
    LOAD_VK(vkCreateBuffer);
    LOAD_VK(vkDestroyBuffer);
    LOAD_VK(vkAllocateMemory);
    LOAD_VK(vkFreeMemory);
    LOAD_VK(vkMapMemory);
    LOAD_VK(vkUnmapMemory);
    LOAD_VK(vkBindBufferMemory);
    LOAD_VK(vkGetBufferMemoryRequirements);
    LOAD_VK(vkCreateImage);
    LOAD_VK(vkDestroyImage);
    LOAD_VK(vkBindImageMemory);
    LOAD_VK(vkGetImageMemoryRequirements);
    LOAD_VK(vkCreateImageView);
    LOAD_VK(vkDestroyImageView);
    LOAD_VK(vkCreateSampler);
    LOAD_VK(vkDestroySampler);
    LOAD_VK(vkCreateShaderModule);
    LOAD_VK(vkDestroyShaderModule);
    LOAD_VK(vkCreateGraphicsPipelines);
    LOAD_VK(vkCreateComputePipelines);
    LOAD_VK(vkDestroyPipeline);
    LOAD_VK(vkCreateCommandPool);
    LOAD_VK(vkDestroyCommandPool);
    LOAD_VK(vkDeviceWaitIdle);

    #undef LOAD_VK

    return vkCreateInstance != nullptr;
}

void VulkanBackend::unload_vulkan_library() {
#ifdef VOID_PLATFORM_WINDOWS
    if (m_vulkan_library) FreeLibrary(m_vulkan_library);
#else
    if (m_vulkan_library) dlclose(m_vulkan_library);
#endif
    m_vulkan_library = nullptr;
}

bool VulkanBackend::create_instance(const BackendConfig& config) {
    struct VkApplicationInfo {
        std::uint32_t sType = 0;  // VK_STRUCTURE_TYPE_APPLICATION_INFO
        const void* pNext = nullptr;
        const char* pApplicationName = "void_engine";
        std::uint32_t applicationVersion = 1;
        const char* pEngineName = "void_render";
        std::uint32_t engineVersion = 1;
        std::uint32_t apiVersion = VK_API_VERSION_1_3;
    } app_info;

    struct VkInstanceCreateInfo {
        std::uint32_t sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        const void* pNext = nullptr;
        VkFlags flags = 0;
        const void* pApplicationInfo = nullptr;
        std::uint32_t enabledLayerCount = 0;
        const char* const* ppEnabledLayerNames = nullptr;
        std::uint32_t enabledExtensionCount = 0;
        const char* const* ppEnabledExtensionNames = nullptr;
    } create_info;

    create_info.pApplicationInfo = &app_info;

    std::vector<const char*> layers;
    if (config.enable_validation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }
    create_info.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();

    return vkCreateInstance && vkCreateInstance(&create_info, nullptr, &m_instance) == VK_SUCCESS;
}

void VulkanBackend::destroy_instance() {
    if (vkDestroyInstance && m_instance) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = nullptr;
    }
}

bool VulkanBackend::select_physical_device() {
    std::uint32_t device_count = 0;
    if (!vkEnumeratePhysicalDevices) return false;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    if (device_count == 0) return false;

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

    // Select first suitable device (production impl would score devices)
    m_physical_device = devices[0];

    // Get queue families
    std::uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);

    struct VkQueueFamilyProperties {
        VkFlags queueFlags = 0;
        std::uint32_t queueCount = 0;
        std::uint32_t timestampValidBits = 0;
        std::uint32_t minImageTransferGranularity[3] = {0, 0, 0};
    };

    std::vector<VkQueueFamilyProperties> families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, families.data());

    for (std::uint32_t i = 0; i < queue_family_count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphics_queue_family = i;
            break;
        }
    }

    return true;
}

bool VulkanBackend::create_device() {
    float queue_priority = 1.0f;

    struct VkDeviceQueueCreateInfo {
        std::uint32_t sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        const void* pNext = nullptr;
        VkFlags flags = 0;
        std::uint32_t queueFamilyIndex = 0;
        std::uint32_t queueCount = 1;
        const float* pQueuePriorities = nullptr;
    } queue_info;

    queue_info.queueFamilyIndex = m_graphics_queue_family;
    queue_info.pQueuePriorities = &queue_priority;

    struct VkDeviceCreateInfo {
        std::uint32_t sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        const void* pNext = nullptr;
        VkFlags flags = 0;
        std::uint32_t queueCreateInfoCount = 1;
        const void* pQueueCreateInfos = nullptr;
        std::uint32_t enabledLayerCount = 0;
        const char* const* ppEnabledLayerNames = nullptr;
        std::uint32_t enabledExtensionCount = 0;
        const char* const* ppEnabledExtensionNames = nullptr;
        const void* pEnabledFeatures = nullptr;
    } create_info;

    create_info.pQueueCreateInfos = &queue_info;

    if (!vkCreateDevice || vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS) {
        return false;
    }

    // Get queue
    if (vkGetDeviceQueue) {
        vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);
    }

    return true;
}

void VulkanBackend::destroy_device() {
    if (vkDestroyDevice && m_device) {
        vkDestroyDevice(m_device, nullptr);
        m_device = nullptr;
    }
}

bool VulkanBackend::create_command_pool() {
    struct VkCommandPoolCreateInfo {
        std::uint32_t sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        const void* pNext = nullptr;
        VkFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        std::uint32_t queueFamilyIndex = 0;
    } create_info;

    create_info.queueFamilyIndex = m_graphics_queue_family;

    return vkCreateCommandPool && vkCreateCommandPool(m_device, &create_info, nullptr, &m_command_pool) == VK_SUCCESS;
}

void VulkanBackend::query_capabilities() {
    struct VkPhysicalDeviceProperties {
        std::uint32_t apiVersion = 0;
        std::uint32_t driverVersion = 0;
        std::uint32_t vendorID = 0;
        std::uint32_t deviceID = 0;
        std::uint32_t deviceType = 0;
        char deviceName[256] = {};
        std::uint8_t pipelineCacheUUID[16] = {};
    } props;

    if (vkGetPhysicalDeviceProperties) {
        vkGetPhysicalDeviceProperties(m_physical_device, &props);
        m_capabilities.device_name = props.deviceName;
        m_capabilities.vendor_id = props.vendorID;
        m_capabilities.device_id = props.deviceID;
    }

    // Vulkan 1.3 features
    m_capabilities.features.compute_shaders = true;
    m_capabilities.features.tessellation = true;
    m_capabilities.features.geometry_shaders = true;
    m_capabilities.features.multi_draw_indirect = true;
    m_capabilities.features.bindless_resources = true;
    m_capabilities.features.timeline_semaphores = true;
    m_capabilities.features.dynamic_rendering = true;
    m_capabilities.features.sampler_anisotropy = true;
}

std::uint32_t VulkanBackend::find_memory_type(VkFlags required_flags) {
    // Simplified - real impl queries VkPhysicalDeviceMemoryProperties
    (void)required_flags;
    return 0;
}

std::uint32_t VulkanBackend::texture_format_to_vk(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8Unorm: return 9;   // VK_FORMAT_R8_UNORM
        case TextureFormat::Rg8Unorm: return 16;  // VK_FORMAT_R8G8_UNORM
        case TextureFormat::Rgba8Unorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::Rgba8UnormSrgb: return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureFormat::Bgra8Unorm: return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::Rgba16Float: return 97;  // VK_FORMAT_R16G16B16A16_SFLOAT
        case TextureFormat::Rgba32Float: return 109; // VK_FORMAT_R32G32B32A32_SFLOAT
        case TextureFormat::Depth32Float: return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::Depth24PlusStencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
        default: return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

} // namespace backends
} // namespace void_render

#endif // VOID_PLATFORM_WINDOWS || VOID_PLATFORM_LINUX
