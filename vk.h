#pragma once
#include "tinytypes.h"
#include "vk_default.h"
#include "helpers.h"
#include <stdint.h>
#include <vulkan/vulkan_core.h>
#include "cachestuff.h"
#define DEBUG

typedef struct
{
    VkInstance instance;
#ifdef DEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif
    VkAllocationCallbacks* allocator;
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

    VkAllocationCallbacks* allocator;


} DeviceContext;

//Touched rarely.

typedef struct VkFeatureChain
{
    VkPhysicalDeviceFeatures2 core;

    VkPhysicalDeviceVulkan11Features v11;
    VkPhysicalDeviceVulkan12Features v12;
    VkPhysicalDeviceVulkan13Features v13;


    // ---- add this ----
    VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5;
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


} RendererDesc;


typedef enum ImageStateValidity
{
    IMAGE_STATE_UNDEFINED = 0,
    IMAGE_STATE_VALID     = 1,
    IMAGE_STATE_EXTERNAL  = 2,
} ImageStateValidity;
typedef struct ImageState
{
    VkImageLayout         layout;
    VkPipelineStageFlags2 stage;
    VkAccessFlags2        access;
    uint32_t              queue_family;
    ImageStateValidity    validity;
} ImageState;

typedef struct Image
{
    VkImage       image;
    VkExtent3D    extent;
    VkFormat      format;
    uint32_t      mipLevels;
    uint32_t      arrayLayers;
    VmaAllocation allocation;

    VkImageUsageFlags usage;
    VkImageView       image_view;
    VkSampler         sampler;

    ImageState state;
} Image;
#define MAX_SWAPCHAIN_IMAGES 8


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

#define MAX_BINDLESS_TEXTURES 65536
#define MAX_BINDLESS_SAMPLERS 256
#define MAX_BINDLESS_STORAGE_BUFFERS 65536
#define MAX_BINDLESS_UNIFORM_BUFFERS 16384
#define MAX_BINDLESS_STORAGE_IMAGES 16384
#define MAX_BINDLESS_VERTEX_BUFFERS 65536
#define MAX_BINDLESS_INDEX_BUFFERS 65536
#define MAX_BINDLESS_MATERIALS 65536
#define MAX_BINDLESS_TRANSFORMS 65536

typedef struct Bindless
{
    VkDescriptorSetLayout set_layout;
    VkDescriptorPool      pool;
    VkDescriptorSet       set;
    VkPipelineLayout pipeline_layout;
} Bindless;
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

    VkAllocationCallbacks* allocator;


    VkSurfaceKHR  surface;
    FlowSwapchain swapchain;
    GLFWwindow*   window;
    FrameContext  frames[MAX_FRAMES_IN_FLIGHT];
    uint32_t      current_frame;
    //
    // DescriptorLayoutCache descriptor_layout_cache;
    // PipelineLayoutCache   pipeline_layout_cache;
    Bindless              bindless_system;

    VkCommandPool one_time_gfx_pool;
    DeviceInfo    info;

    VkPipelineCache pipeline_cache;

    Image depth;
} Renderer;

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
static GraphicsPipelineConfig pipeline_config_default(void)
{
    GraphicsPipelineConfig cfg = {0};

    cfg.cull_mode    = VK_CULL_MODE_NONE;
    cfg.front_face   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    cfg.polygon_mode = VK_POLYGON_MODE_FILL;

    cfg.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    cfg.depth_test_enable  = true;
    cfg.depth_write_enable = true;
    cfg.depth_compare_op   = VK_COMPARE_OP_LESS_OR_EQUAL;

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

void vk_create_swapchain(VkDevice                       device,
                         VkPhysicalDevice               gpu,
                         FlowSwapchain*                 out_swapchain,
                         const FlowSwapchainCreateInfo* info,
                         VkQueue                        graphics_queue,
                         VkCommandPool                  one_time_pool);
void vk_swapchain_destroy(VkDevice device, FlowSwapchain* swapchain);
bool vk_swapchain_acquire(VkDevice device, FlowSwapchain* sc, VkSemaphore image_available, VkFence fence, uint64_t timeout);

bool vk_swapchain_present(VkQueue present_queue, FlowSwapchain* sc, const VkSemaphore* waits, uint32_t wait_count);

void vk_swapchain_recreate(VkDevice device, VkPhysicalDevice gpu, FlowSwapchain* sc, uint32_t new_w, uint32_t new_h, VkQueue graphics_queue, VkCommandPool one_time_pool);
VkPresentModeKHR vk_swapchain_select_present_mode(VkPhysicalDevice physical_device, VkSurfaceKHR surface, bool vsync);


void image_transition(VkCommandBuffer cmd, Image* image, VkImageLayout new_layout, VkPipelineStageFlags2 new_stage, VkAccessFlags2 new_access);

void image_transition_swapchain(VkCommandBuffer cmd, FlowSwapchain* sc, VkImageLayout new_layout, VkPipelineStageFlags2 new_stage, VkAccessFlags2 new_access);

void image_transition_simple(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout, VkImageLayout new_layout);


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
