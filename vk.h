#pragma once
#define IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE (1)
#define IMGUI_IMPL_VULKAN_USE_VOLK
#define CIMGUI_USE_GLFW
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_VULKAN


#include "vk_default.h"
#ifdef Status
#undef Status
#endif


#include "tinytypes.h"

#include "external/cimgui/cimgui.h"

#include "external/cimgui/cimgui_impl.h"


#include "helpers.h"
#include "offset_allocator.h"
#include <stdint.h>
#include "cachestuff.h"
#include "flow/flow.h"
// Fallback for older Vulkan headers without VK_KHR_shader_non_semantic_info
#ifndef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_NON_SEMANTIC_INFO_FEATURES_KHR
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_NON_SEMANTIC_INFO_FEATURES_KHR 1000333000
#endif

#ifndef VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
#define VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME "VK_KHR_shader_non_semantic_info"
#endif

#ifndef VK_PHYSICAL_DEVICE_SHADER_NON_SEMANTIC_INFO_FEATURES_KHR
#define VK_PHYSICAL_DEVICE_SHADER_NON_SEMANTIC_INFO_FEATURES_KHR 1
typedef struct VkPhysicalDeviceShaderNonSemanticInfoFeaturesKHR
{
    VkStructureType sType;
    void*           pNext;
    VkBool32        shaderNonSemanticInfo;
} VkPhysicalDeviceShaderNonSemanticInfoFeaturesKHR;
#endif


#define MAX_MIPS 16
#define MAX_SWAPCHAIN_IMAGES 8

#define MAX_BINDLESS_TEXTURES 65536
#define MAX_BINDLESS_SAMPLERS 256
#define MAX_BINDLESS_STORAGE_BUFFERS 65536
#define MAX_BINDLESS_UNIFORM_BUFFERS 16384
#define MAX_BINDLESS_STORAGE_IMAGES 16384
#define MAX_BINDLESS_VERTEX_BUFFERS 65536
#define MAX_BINDLESS_INDEX_BUFFERS 65536
#define MAX_BINDLESS_MATERIALS 65536
#define MAX_BINDLESS_TRANSFORMS 65536


typedef struct
{
    VkInstance instance;
#ifdef DEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif

    VkAllocationCallbacks* allocatorcallbacks;
} InstanceContext;


typedef struct
{
    VkPhysicalDevice physical;
    //warm data
    VkDevice device;

    VkQueue present_queue;
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;

    uint32_t present_queue_index;
    uint32_t graphics_queue_index;
    uint32_t compute_queue_index;
    uint32_t transfer_queue_index;


    VkAllocationCallbacks* allocatorcallbacks;

} DeviceContext;

//Touched rarely.

typedef struct VkFeatureChain
{
    VkPhysicalDeviceFeatures2 core;

    VkPhysicalDeviceVulkan11Features v11;
    VkPhysicalDeviceVulkan12Features v12;
    VkPhysicalDeviceVulkan13Features v13;


    // ---- add this ----
    VkPhysicalDeviceMaintenance5FeaturesKHR          maintenance5;
    VkPhysicalDeviceShaderNonSemanticInfoFeaturesKHR shaderNonSemanticInfo;
} VkFeatureChain;


typedef struct
{
    VkPhysicalDevice physical;

    VkPhysicalDeviceProperties       properties;
    VkPhysicalDeviceFeatures         features;
    VkPhysicalDeviceMemoryProperties memory;

    VkFeatureChain feature_chain;

} DeviceInfo;
typedef struct
{

    uint32_t width;
    uint32_t height;

    const char* app_name;

    const char** instance_layers;
    const char** instance_extensions;
    const char** device_extensions;

    uint32_t instance_layer_count;
    uint32_t instance_extension_count;
    uint32_t device_extension_count;
    bool     enable_validation;
    bool     enable_gpu_based_validation;

    VkDebugUtilsMessageSeverityFlagsEXT validation_severity;
    VkDebugUtilsMessageTypeFlagsEXT     validation_types;
    bool                                use_custom_features;
    VkFeatureChain                      custom_features;
    VkPresentModeKHR                    swapchain_preferred_presest_mode;
    VkFormat                            swapchain_preferred_format;

    VkColorSpaceKHR swapchain_preferred_color_space;

    VkImageUsageFlags swapchain_extra_usage_flags; /* Additional usage flags */
    bool              vsync;
    bool              enable_debug_printf; /* Enable VK_KHR_shader_non_semantic_info for shader debug printf */
    uint32_t          bindless_sampled_image_count;
    uint32_t          bindless_sampler_count;
    uint32_t          bindless_storage_image_count;


} RendererDesc;


typedef enum ImageStateValidity
{
    IMAGE_STATE_UNDEFINED = 0,
    IMAGE_STATE_VALID     = 1,
    IMAGE_STATE_EXTERNAL  = 2,
} ImageStateValidity;


typedef struct ALIGNAS(32) ImageState
{
    VkPipelineStageFlags2 stage;         // 8
    VkAccessFlags2        access;        // 8
    VkImageLayout         layout;        // 4
    uint32_t              queue_family;  // 4
    ImageStateValidity    validity;      // 4
    uint32_t              dirty_mips;    // 4
} ImageState;


//
// for each texture in pool
// {
//     if texture.requestedMip < texture.residentMip
//         stream_in(texture)
//
//     if texture.requestedMip > texture.residentMip
//         stream_out(texture)
// }
//


typedef struct ALIGNAS(64) FlowSwapchain
{
    //hot

    ImageState states[MAX_SWAPCHAIN_IMAGES];
    VkImage    images[MAX_SWAPCHAIN_IMAGES];
    uint32_t   current_image;

    VkImageView image_views[MAX_SWAPCHAIN_IMAGES];


    VkSemaphore render_finished[MAX_SWAPCHAIN_IMAGES];
    //cold
    VkSwapchainKHR   swapchain;
    VkSurfaceKHR     surface;
    VkFormat         format;
    VkColorSpaceKHR  color_space;
    VkPresentModeKHR present_mode;
    VkExtent2D       extent;
    uint32_t         image_count;

    VkImageUsageFlags image_usage;

    bool vsync;
    bool needs_recreate;
} FlowSwapchain;


typedef struct FlowSwapchainCreateInfo
{
    VkSurfaceKHR      surface;
    uint32_t          width;
    uint32_t          height;
    uint32_t          min_image_count;
    VkPresentModeKHR  preferred_present_mode;
    VkFormat          preferred_format;
    VkColorSpaceKHR   preferred_color_space; /* VK_COLOR_SPACE_SRGB_NONLINEAR_KHR default */
    VkImageUsageFlags extra_usage;           /* Additional usage flags */
    VkSwapchainKHR    old_swapchain;         /* For recreation */
} FlowSwapchainCreateInfo;

typedef struct
{
    VkCommandBuffer cmdbuf;
    VkCommandPool   cmdbufpool;
    VkSemaphore     image_available_semaphore;
    VkFence         in_flight_fence;
} FrameContext;

#pragma once


typedef struct Bindless
{
    VkDescriptorSetLayout set_layout;
    VkDescriptorPool      pool;
    VkDescriptorSet       set;

    VkPipelineLayout pipeline_layout;

} Bindless;


#define MAX_DEFAULT_SAMPLERS 16

typedef enum DefaultSamplerID
{
    SAMPLER_LINEAR_WRAP = 0,
    SAMPLER_LINEAR_CLAMP,
    SAMPLER_NEAREST_WRAP,
    SAMPLER_NEAREST_CLAMP,
    SAMPLER_LINEAR_WRAP_ANISO,
    SAMPLER_SHADOW,
    SAMPLER_COUNT
} DefaultSamplerID;

typedef struct DefaultSamplerTable
{
    VkSampler samplers[SAMPLER_COUNT];
} DefaultSamplerTable;

// ────────────────────────────────────────────────────────────────
// Render Targets
// ────────────────────────────────────────────────────────────────

#define RT_MAX_MIPS 13  // covers up to 4096x4096
#define RT_POOL_MAX 64

typedef struct RenderTarget
{
    VkImage       image;
    VmaAllocation allocation;
    VkImageView   view;                    // full mip chain view (for sampling)
    VkImageView   mip_views[RT_MAX_MIPS];  // per-mip views (for attachments)

    VkFormat              format;
    uint32_t              width;
    uint32_t              height;
    uint32_t              mip_count;
    VkSampleCountFlagBits samples;  // 0 = VK_SAMPLE_COUNT_1_BIT
    VkImageUsageFlags     usage;
    VkImageAspectFlags    aspect;

    // Per-mip sync state — reuses existing ImageState
    ImageState mip_states[RT_MAX_MIPS];

    // Bindless integration
    uint32_t sampled_id;  // bindless sampled image slot (UINT32_MAX if unused)
    uint32_t storage_id;  // bindless storage image slot (UINT32_MAX if unused)
} RenderTarget;


typedef struct RenderTargetPool
{
    RenderTarget targets[RT_POOL_MAX];
    bool         in_use[RT_POOL_MAX];
    uint32_t     unused_frames[RT_POOL_MAX];
    uint32_t     count;
} RenderTargetPool;
typedef enum FrustumPlane
{
    TopPlane,
    BottomPlane,
    LeftPlane,
    RightPlane,
    NearPlane,
    FarPlane,
    FrustumPlaneCount
} FrustumPlane;

typedef struct Frustum
{
    //  ax + by + cz + d = 0
    //  vec =(a,b,c,d)
    vec4 planes[FrustumPlaneCount];
} Frustum;


#define MAX_FRAMES_IN_FLIGHT 3
typedef struct
{
    InstanceContext  instance;
    VkPhysicalDevice physical_device;
    //warm data
    VkDevice device;

    VkQueue present_queue;
    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;

    uint32_t present_queue_index;
    uint32_t graphics_queue_index;
    uint32_t compute_queue_index;
    uint32_t transfer_queue_index;

    VkAllocationCallbacks* allocatorcallbacks;
    VmaAllocator           vmaallocator;

    VkSurfaceKHR  surface;
    FlowSwapchain swapchain;
    GLFWwindow*   window;
    FrameContext  frames[MAX_FRAMES_IN_FLIGHT];
    uint32_t      current_frame;


    // DescriptorLayoutCache descriptor_layout_cache;
    // PipelineLayoutCache   pipeline_layout_cache;
    Bindless bindless_system;

    VkCommandPool one_time_gfx_pool;
    VkCommandPool transfer_pool;

    DeviceInfo info;

    VkPipelineCache  pipeline_cache;
    Frustum          frustum;
    VkDescriptorPool imgui_pool;
    RenderTarget     hdr_color;
    RenderTarget     depth;

    flow_id_pool texture_pool;
    //  RenderTarget depth[MAX_SWAPCHAIN_IMAGES];
} Renderer;

typedef struct BufferPool
{
    VkBuffer                 buffer;
    VmaAllocation            allocation;
    VkDeviceSize             size_bytes;
    VkBufferUsageFlags       usage;
    VmaMemoryUsage           memory_usage;
    VmaAllocationCreateFlags alloc_flags;
    void*                    mapped;
    OA_Allocator             allocator;
} BufferPool;

typedef struct BufferSlice
{
    BufferPool*   pool;
    VkBuffer      buffer;
    VkDeviceSize  offset;
    VkDeviceSize  size;
    void*         mapped;
    OA_Allocation allocation;
} BufferSlice;

bool        buffer_pool_init(Renderer*                r,
                             BufferPool*              pool,
                             VkDeviceSize             size_bytes,
                             VkBufferUsageFlags       usage,
                             VmaMemoryUsage           memory_usage,
                             VmaAllocationCreateFlags alloc_flags,
                             oa_uint32                max_allocs);
void        buffer_pool_destroy(Renderer* r, BufferPool* pool);
BufferSlice buffer_pool_alloc(BufferPool* pool, VkDeviceSize size_bytes, VkDeviceSize alignment);
void        buffer_pool_free(BufferSlice slice);

#define MAX_VERTEX_ATTRS 8
typedef enum VertexFormat
{
    FMT_FLOAT,
    FMT_VEC2,
    FMT_VEC3,
    FMT_VEC4,
} VertexFormat;

typedef struct VertexAttr
{
    uint8_t      location;
    VertexFormat format;
} VertexAttr;

typedef struct VertexBinding
{
    uint16_t   stride;
    uint8_t    input_rate;  // VK_VERTEX_INPUT_RATE_VERTEX / INSTANCE
    uint8_t    attr_count;
    VertexAttr attrs[MAX_VERTEX_ATTRS];
} VertexBinding;

#define MAX_COLOR_ATTACHMENTS 8

typedef struct ColorAttachmentBlend
{
    bool blend_enable;

    VkBlendFactor src_color;
    VkBlendFactor dst_color;
    VkBlendOp     color_op;

    VkBlendFactor src_alpha;
    VkBlendFactor dst_alpha;
    VkBlendOp     alpha_op;

    VkColorComponentFlags write_mask;

} ColorAttachmentBlend;


typedef struct GraphicsPipelineConfig
{
    // Rasterization
    //
    const char*     vert_path;
    const char*     frag_path;
    VkCullModeFlags cull_mode;
    VkFrontFace     front_face;
    VkPolygonMode   polygon_mode;

    VkPrimitiveTopology topology;

    bool        depth_test_enable;
    bool        depth_write_enable;
    VkCompareOp depth_compare_op;

    uint32_t        color_attachment_count;
    const VkFormat* color_formats;
    VkFormat        depth_format;

    // Per-attachment blend state
    ColorAttachmentBlend blends[MAX_COLOR_ATTACHMENTS];
    // Vertex input (optional)
    bool          use_vertex_input;
    VertexBinding vertex_binding;

} GraphicsPipelineConfig;
static ColorAttachmentBlend blend_alpha(void)
{
    return (ColorAttachmentBlend){
        .blend_enable = true,

        .src_color = VK_BLEND_FACTOR_SRC_ALPHA,
        .dst_color = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .color_op  = VK_BLEND_OP_ADD,

        .src_alpha = VK_BLEND_FACTOR_ONE,
        .dst_alpha = VK_BLEND_FACTOR_ZERO,
        .alpha_op  = VK_BLEND_OP_ADD,

        .write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
}

static ColorAttachmentBlend blend_additive(void)
{
    return (ColorAttachmentBlend){
        .blend_enable = true,

        .src_color = VK_BLEND_FACTOR_ONE,
        .dst_color = VK_BLEND_FACTOR_ONE,
        .color_op  = VK_BLEND_OP_ADD,

        .src_alpha = VK_BLEND_FACTOR_ONE,
        .dst_alpha = VK_BLEND_FACTOR_ONE,
        .alpha_op  = VK_BLEND_OP_ADD,

        .write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
}


static ColorAttachmentBlend blend_disabled(void)
{
    return (ColorAttachmentBlend){
        .blend_enable = false,

        .src_color = VK_BLEND_FACTOR_ONE,
        .dst_color = VK_BLEND_FACTOR_ZERO,
        .color_op  = VK_BLEND_OP_ADD,

        .src_alpha = VK_BLEND_FACTOR_ONE,
        .dst_alpha = VK_BLEND_FACTOR_ZERO,
        .alpha_op  = VK_BLEND_OP_ADD,

        .write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
}
static inline GraphicsPipelineConfig pipeline_config_default(void)
{
    GraphicsPipelineConfig cfg = {0};

    cfg.cull_mode    = VK_CULL_MODE_NONE;
    cfg.front_face   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    cfg.polygon_mode = VK_POLYGON_MODE_FILL;

    cfg.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    cfg.depth_test_enable  = true;
    cfg.depth_write_enable = true;
    cfg.depth_compare_op   = VK_COMPARE_OP_GREATER;

    cfg.color_attachment_count = 0;
    cfg.color_formats          = NULL;
    cfg.depth_format           = VK_FORMAT_UNDEFINED;

    for(uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++)
        cfg.blends[i] = blend_disabled();

    cfg.use_vertex_input = false;

    return cfg;
}

VkPipeline create_graphics_pipeline(Renderer* renderer, const GraphicsPipelineConfig* cfg);
VkPipeline create_compute_pipeline(Renderer* renderer, const char* compute_path);

void vk_cmd_set_viewport_scissor(VkCommandBuffer cmd, VkExtent2D extent);


typedef enum SwapchainResult
{
    SWAPCHAIN_OK,
    SWAPCHAIN_SUBOPTIMAL,
    SWAPCHAIN_OUT_OF_DATE,
} SwapchainResult;
bool is_instance_extension_supported(const char* extension_name);
void renderer_create(Renderer* r, RendererDesc* desc);

void renderer_destroy(Renderer* r);
void vk_create_swapchain(VkDevice                       device,
                         VkPhysicalDevice               gpu,
                         FlowSwapchain*                 out_swapchain,
                         const FlowSwapchainCreateInfo* info,
                         VkQueue                        graphics_queue,
                         VkCommandPool                  one_time_pool);
void vk_swapchain_destroy(VkDevice device, FlowSwapchain* swapchain);

void vk_swapchain_recreate(VkDevice device, VkPhysicalDevice gpu, FlowSwapchain* sc, uint32_t new_w, uint32_t new_h, VkQueue graphics_queue, VkCommandPool one_time_pool);
VkPresentModeKHR vk_swapchain_select_present_mode(VkPhysicalDevice physical_device, VkSurfaceKHR surface, bool vsync);


void image_transition_swapchain(VkCommandBuffer cmd, FlowSwapchain* sc, VkImageLayout new_layout, VkPipelineStageFlags2 new_stage, VkAccessFlags2 new_access);

void image_transition_simple(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout, VkImageLayout new_layout);


static inline VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspect, uint32_t baseMip, uint32_t mipCount)
{
    VkImageSubresourceRange range = {
        .aspectMask = aspect, .baseMipLevel = baseMip, .levelCount = mipCount, .baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS};

    return range;
}
void cmd_transition_all_mips(VkCommandBuffer       cmd,
                             VkImage               image,
                             ImageState*           state,
                             VkImageAspectFlags    aspect,
                             uint32_t              mipCount,
                             VkPipelineStageFlags2 newStage,
                             VkAccessFlags2        newAccess,
                             VkImageLayout         newLayout,
                             uint32_t              newQueueFamilyi);

void cmd_transition_mip(VkCommandBuffer       cmd,
                        VkImage               image,
                        ImageState*           state,
                        VkImageAspectFlags    aspect,
                        uint32_t              mip,
                        VkPipelineStageFlags2 newStage,
                        VkAccessFlags2        newAccess,
                        VkImageLayout         newLayout,
                        uint32_t              newQueueFamily);


FORCE_INLINE bool vk_swapchain_acquire(VkDevice device, FlowSwapchain* sc, VkSemaphore image_available, VkFence fence, uint64_t timeout)
{
    VkResult r = vkAcquireNextImageKHR(device, sc->swapchain, timeout, image_available, fence, &sc->current_image);

    if(r == VK_SUCCESS)
        return true;

    if(r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR)
    {
        sc->needs_recreate = true;
        return false;
    }

    VK_CHECK(r);
    return false;
}

FORCE_INLINE bool vk_swapchain_present(VkQueue present_queue, FlowSwapchain* sc, const VkSemaphore* waits, uint32_t wait_count)
{
    VkPresentInfoKHR info = {.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                             .waitSemaphoreCount = wait_count,
                             .pWaitSemaphores    = waits,
                             .swapchainCount     = 1,
                             .pSwapchains        = &sc->swapchain,
                             .pImageIndices      = &sc->current_image};

    VkResult r = vkQueuePresentKHR(present_queue, &info);

    if(r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR)
    {
        sc->needs_recreate = true;
        return false;
    }

    VK_CHECK(r);
    return true;
}

typedef struct
{
    uint32_t handle;
    uint32_t generation;
} BufferHandle;
typedef struct
{
    uint32_t handle;
    uint32_t generation;
} TextureHandle;

//
// typedef enum
// {
//     TEXTURE_STREAMING_IDLE,
//     TEXTURE_STREAMING_IN,
//     TEXTURE_STREAMING_OUT
// } TextureStreamingState;
//
//
// typedef enum
// {
//     TEXTURE_RESIDENCY_NONE,
//     TEXTURE_RESIDENCY_PARTIAL,
//     TEXTURE_RESIDENCY_FULL
// } TextureResidency;
//
//
// // ------------------------------------------------------------
// // TEXTURE OBJECT
// // ------------------------------------------------------------
//
// typedef struct
// {
//     VkImage       image;
//     VmaAllocation allocation;
//     VkImageView   view;
//
//     uint32_t width;
//     uint32_t height;
//     uint32_t mipCount;
//
//     uint32_t residentMip;
//     uint32_t requestedMip;
//
//     TextureResidency      residency;
//     TextureStreamingState streaming;
//
//
//     ImageState state;
//
//     uint32_t generation;
//
// } Texture;
//
// // ------------------------------------------------------------
// // POOL
// // ------------------------------------------------------------
//
// typedef struct
// {
//     Texture textures[MAX_BINDLESS_TEXTURES];
//
//     uint32_t freeList[MAX_BINDLESS_TEXTURES];
//     uint32_t freeCount;
//
// } TexturePool;
//
// typedef struct TextureStreamRequest
// {
//     TextureHandle handle;
//     uint32_t      mip;
// } TextureStreamRequest;
//
// static Texture* texture_get(TextureHandle handle, TexturePool* pool)
// {
//     Texture* tex = &pool->textures[handle.index];
//
//     if(tex->generation != handle.generation)
//         return NULL;
//
//     return tex;
// }
//
//
// static TextureHandle texture_alloc_handle(TexturePool* pool)
// {
//     assert(pool->freeCount > 0);
//
//     uint32_t index = pool->freeList[--pool->freeCount];
//
//     Texture* tex = &pool->textures[index];
//
//     tex->generation++;
//
//     tex->residentMip  = UINT32_MAX;
//     tex->requestedMip = UINT32_MAX;
//
//     tex->residency = TEXTURE_RESIDENCY_NONE;
//     tex->streaming = TEXTURE_STREAMING_IDLE;
//
//     tex->state.validity = IMAGE_STATE_UNDEFINED;
//
//     TextureHandle h = {.index = index, .generation = tex->generation};
//
//     return h;
// }
//
//
// // ------------------------------------------------------------
// // INIT
// // ------------------------------------------------------------
//
//
// void texture_system_init(TexturePool* pool)
// {
//     memset(pool, 0, sizeof(*pool));
//
//     pool->freeCount = MAX_BINDLESS_TEXTURES;
//
//     for(uint32_t i = 0; i < MAX_BINDLESS_TEXTURES; i++)
//         pool->freeList[i] = i;
// }
// void texture_stream_in(TextureHandle handle,
//                        uint32_t      mip,
//                        VkBuffer      stagingBuffer,
//                        VkDeviceSize  offset,
//                        TexturePool*  pool,
//                        VkDevice      device,
//                        VkCommandPool transferpool,
//                        VkQueue       transferqueue)
// {
//     Texture* tex = texture_get(handle, pool);
//
//     VkCommandBuffer cmd = begin_one_time_cmd(device, transferpool);
//
//
//     cmd_transition_mip(cmd, tex->image, &tex->state, VK_IMAGE_ASPECT_COLOR_BIT, mip, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
//                        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED);
//
//     uint32_t w = tex->width >> mip;
//     uint32_t h = tex->height >> mip;
//
//     if(w == 0)
//         w = 1;
//     if(h == 0)
//         h = 1;
//
//
//     VkBufferImageCopy copy = {
//         .bufferOffset = offset,
//
//         .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip, .baseArrayLayer = 0, .layerCount = 1},
//         .imageExtent = {w, h, 1}};
//
//     vkCmdCopyBufferToImage(cmd, stagingBuffer, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
//
//     cmd_transition_mip(cmd, tex->image, &tex->state, VK_IMAGE_ASPECT_COLOR_BIT, mip, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
//                        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED);
//     end_one_time_cmd(device, transferqueue, transferpool, cmd);
//
//     tex->residentMip = mip;
//
//     if(mip == 0)
//         tex->residency = TEXTURE_RESIDENCY_FULL;
// }
//
//
// // ------------------------------------------------------------
// // UPDATE STREAMING
// // ------------------------------------------------------------
//
// uint32_t texture_streaming_update(TexturePool* pool, TextureStreamRequest* requests, uint32_t maxRequests)
// {
//     uint32_t count = 0;
//
//     for(uint32_t i = 0; i < MAX_BINDLESS_TEXTURES; i++)
//     {
//         Texture* tex = &pool->textures[i];
//
//         if(tex->generation == 0)
//             continue;
//
//         if(tex->requestedMip < tex->residentMip)
//         {
//             tex->streaming = TEXTURE_STREAMING_IN;
//
//             if(count < maxRequests)
//             {
//                 requests[count++] = (TextureStreamRequest){.handle = {i, tex->generation}, .mip = tex->residentMip - 1};
//             }
//         }
//         else if(tex->requestedMip > tex->residentMip)
//         {
//             tex->streaming = TEXTURE_STREAMING_OUT;
//         }
//         else
//         {
//             tex->streaming = TEXTURE_STREAMING_IDLE;
//         }
//     }
//
//     return count;
// }
// void texture_destroy(TextureHandle handle, TexturePool* pool, Renderer* r)
// {
//     Texture* tex = texture_get(handle, pool);
//
//     if(!tex)
//         return;
//
//     vkDestroyImageView(r->device, tex->view, NULL);
//     vmaDestroyImage(r->vmaallocator, tex->image, tex->allocation);
//     tex->generation++;  // ← CRITICAL
//     pool->freeList[pool->freeCount++] = handle.index;
//
//
//     VkDescriptorImageInfo info = {.imageView = VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};
//
//     VkWriteDescriptorSet write = {.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//                                   .dstSet          = r->bindless_system.set,
//                                   .dstBinding      = 0,
//                                   .dstArrayElement = handle.index,
//                                   .descriptorCount = 1,
//                                   .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
//                                   .pImageInfo      = &info};
//
//     vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
// }
//
// void texture_bind(TextureHandle handle, TexturePool* pool, Renderer* r)
// {
//     Texture* tex = texture_get(handle, pool);
//
//     VkDescriptorImageInfo info = {.imageView = tex->view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
//
//     VkWriteDescriptorSet write = {.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//                                   .dstSet          = r->bindless_system.set,
//                                   .dstBinding      = 0,
//                                   .dstArrayElement = handle.index,
//                                   .descriptorCount = 1,
//                                   .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
//                                   .pImageInfo      = &info};
//
//     vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
// }
//
// TextureHandle texture_create(TexturePool* pool, Renderer* r, uint32_t width, uint32_t height, uint32_t mipCount, VkFormat format)
// {
//     TextureHandle handle = texture_alloc_handle(pool);
//
//     Texture* tex = texture_get(handle, pool);
//
//     tex->width    = width;
//     tex->height   = height;
//     tex->mipCount = mipCount;
//
//     tex->residentMip  = mipCount - 1;
//     tex->requestedMip = mipCount - 1;
//
//     tex->residency = TEXTURE_RESIDENCY_PARTIAL;
//
//     VkImageCreateInfo imageInfo = {.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//                                    .imageType   = VK_IMAGE_TYPE_2D,
//                                    .format      = format,
//                                    .extent      = {width, height, 1},
//                                    .mipLevels   = mipCount,
//                                    .arrayLayers = 1,
//                                    .samples     = VK_SAMPLE_COUNT_1_BIT,
//                                    .tiling      = VK_IMAGE_TILING_OPTIMAL,
//                                    .usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};
//
//     VmaAllocationCreateInfo allocInfo = {.usage = VMA_MEMORY_USAGE_GPU_ONLY};
//
//     vmaCreateImage(r->vmaallocator, &imageInfo, &allocInfo, &tex->image, &tex->allocation, NULL);
//
//     VkImageViewCreateInfo viewInfo = {
//         .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
//         .image    = tex->image,
//         .viewType = VK_IMAGE_VIEW_TYPE_2D,
//         .format   = format,
//         .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipCount, .layerCount = 1}};
//
//     vkCreateImageView(r->device, &viewInfo, NULL, &tex->view);
//
//     texture_bind(handle, pool, r);
//
//     return handle;
// }
//
//Instance → GPU selection → Query capabilities → Enable features → Create logical device → Store result
//
// Wait for frame fence
// Acquire swapchain image
// Reset frame command buffer
// Record command buffer
// Submit command buffer
// Present swapchain image
// Advance frame index
//
//
//
//
//
//
//
//
//
//
//
FORCE_INLINE void imgui_shutdown(void)
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    igDestroyContext(NULL);
}

FORCE_INLINE void imgui_begin_frame(void)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    igNewFrame();
}
