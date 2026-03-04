#pragma once
#define IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE (1)
#define IMGUI_IMPL_VULKAN_USE_VOLK
#define CIMGUI_USE_GLFW
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_VULKAN
#define CGLM_ALL_UNALIGNED
#include "external/cglm/include/cglm/cglm.h"
#include "external/cglm/include/cglm/types.h"

#include "vk_default.h"
#ifdef Status
#undef Status
#endif
#include "gpu_timer.h"
#include "flow/flow.h"

#include "tinytypes.h"

#include "external/cimgui/cimgui.h"

#include "external/cimgui/cimgui_impl.h"
#define internal static
#define global static
#define local_persist static

#include "helpers.h"
#include "offset_allocator.h"

#include "external/stb/stb_image.h"
#include <stdint.h>
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


#define BINDLESS_TEXTURE_BINDING 0
#define BINDLESS_SAMPLER_BINDING 1
#define BINDLESS_STORAGE_IMAGE_BINDING 2
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


typedef uint32_t TextureID;
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
    bool              enable_pipeline_stats;
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


// ────────────────────────────────────────────────────────────────
// Render Targets
// ────────────────────────────────────────────────────────────────

#define RT_MAX_MIPS 13  // covers up to 4096x4096
#define RT_POOL_MAX 64

typedef struct RenderTarget
{
    VkImage       image;
    VmaAllocation allocation;

    VkImageView view;                    // full mip chain
    VkImageView mip_views[RT_MAX_MIPS];  // per-mip views

    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t layers;
    uint32_t mip_count;

    VkImageUsageFlags  usage;
    VkImageAspectFlags aspect;

    ImageState mip_states[RT_MAX_MIPS];

    uint32_t bindless_index;  // shared index for sampled/storage

    const char* debug_name;

} RenderTarget;


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
//
//  shader decides which sampler to use.
//
// This is actually powerful. You can do things like:
//
// same texture
// different samplers
//
// Example:
//
// • albedo texture → linear filtering
// • pixel-art texture → nearest filtering
// • shadow map → comparison sampler
typedef struct
{
    VkImage       image;
    VkImageView   view;
    VmaAllocation allocation;

    // metadata
    uint32_t width;
    uint32_t height;
    uint32_t mip_count;

    VkFormat format;

    bool valid;
} Texture;


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


    Bindless bindless_system;

    VkCommandPool one_time_gfx_pool;
    VkCommandPool transfer_pool;

    DeviceInfo info;

    VkPipelineCache  pipeline_cache;
    Frustum          frustum;
    VkDescriptorPool imgui_pool;
    RenderTarget     depth[MAX_SWAPCHAIN_IMAGES];      // per-image depth
    RenderTarget     hdr_color[MAX_SWAPCHAIN_IMAGES];  // optional HDR buffer

    GpuProfiler gpuprofiler[MAX_FRAMES_IN_FLIGHT];

    flow_id_pool texture_pool;
    flow_id_pool sampler_pool;
    Texture      textures[MAX_BINDLESS_TEXTURES];  // reference by textureid
    VkSampler    sampler[MAX_BINDLESS_SAMPLERS];   // reference by textureid
    TextureID    dummy_texture;

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
    cfg.depth_compare_op   = VK_COMPARE_OP_LESS;

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

//Instance → GPU selection → Query capabilities → Enable features → Create logical device → Store result
//
// Wait for frame fence
// Acquire swapchain image
// Reset frame command buffer
// Record command buffer
// Submit command buffer
// Present swapchain image
// Advance frame index


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


typedef struct
{
    vec3  position;
    float yaw;    // radians
    float pitch;  // radians
    float move_speed;
    float look_speed;
    float fov_y;
    float near_z;
    float far_z;
} Camera;

static FORCE_INLINE void camera_build_proj_reverse_z_infinite(mat4 out_proj, Camera* cam, float aspect)
{
    float f = 1.0f / tanf(cam->fov_y * 0.5f);
    float n = cam->near_z;

    glm_mat4_zero(out_proj);

    out_proj[0][0] = f / aspect;
    out_proj[1][1] = f;

    // Reverse-Z, infinite far
    out_proj[2][2] = 0.0f;
    out_proj[2][3] = -1.0f;

    out_proj[3][2] = n;
    out_proj[3][3] = 0.0f;
}


typedef struct Buffer
{
    VkBuffer        buffer;
    VkDeviceSize    buffer_size;
    VkDeviceAddress address;
    uint8_t*        mapping;
    VmaAllocation   allocation;
} Buffer;

bool create_buffer(Renderer* r, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, Buffer* out);

void destroy_buffer(Renderer* r, Buffer* b);


typedef struct
{
    uint32_t          width;
    uint32_t          height;
    uint32_t          mip_count;
    VkFormat          format;
    VkImageUsageFlags usage;
    const char*       debug_name;
} TextureCreateDesc;

TextureID create_texture(Renderer* r, const TextureCreateDesc* desc);
void      destroy_texture(Renderer* r, TextureID id);

TextureID load_texture(Renderer* r, const char* path);

typedef uint32_t SamplerID;

typedef struct
{
    VkFilter mag_filter;
    VkFilter min_filter;

    VkSamplerAddressMode address_u;
    VkSamplerAddressMode address_v;
    VkSamplerAddressMode address_w;

    VkSamplerMipmapMode mipmap_mode;

    float max_lod;

    const char* debug_name;

} SamplerCreateDesc;

SamplerID create_sampler(Renderer* r, const SamplerCreateDesc* desc);
void      destroy_sampler(Renderer* r, SamplerID id);


FLOW_INLINE void rt_transition_mip(VkCommandBuffer cmd, RenderTarget* rt, uint32_t mip, VkPipelineStageFlags2 new_stage, VkAccessFlags2 new_access, VkImageLayout new_layout)
{
    assert(mip < rt->mip_count);
    cmd_transition_mip(cmd, rt->image, &rt->mip_states[mip], rt->aspect, mip, new_stage, new_access, new_layout, VK_QUEUE_FAMILY_IGNORED);
}

FLOW_INLINE void rt_transition_all(VkCommandBuffer cmd, RenderTarget* rt, VkPipelineStageFlags2 new_stage, VkAccessFlags2 new_access, VkImageLayout new_layout)
{
    for(uint32_t mip = 0; mip < rt->mip_count; mip++)
    {
        ImageState* s = &rt->mip_states[mip];
        // Skip if already in target state
        if(s->validity == IMAGE_STATE_VALID && s->stage == new_stage && s->access == new_access && s->layout == new_layout)
        {
            continue;
        }
        cmd_transition_mip(cmd, rt->image, s, rt->aspect, mip, new_stage, new_access, new_layout, VK_QUEUE_FAMILY_IGNORED);
    }
}


typedef uint32_t RenderTargetID;
typedef struct RenderTargetSpec
{
    uint32_t           width;
    uint32_t           height;
    uint32_t           layers;
    VkFormat           format;
    VkImageUsageFlags  usage;
    VkImageAspectFlags aspect;     // 0 = infer from format
    uint32_t           mip_count;  // 0 = auto-compute, 1 = no mips
    const char*        debug_name;
} RenderTargetSpec;


bool rt_create(Renderer* r, RenderTarget* rt, const RenderTargetSpec* spec);
bool rt_resize(Renderer* r, RenderTarget* rt, uint32_t width, uint32_t height);
void rt_destroy(Renderer* r, RenderTarget* rt);
