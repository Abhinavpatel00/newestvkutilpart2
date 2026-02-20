#include "tinytypes.h"
#include "vk_default.h"
#include "vk.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdio.h>
#include <vulkan/vulkan_core.h>

#define CACHELINE_SIZE 64

#define ANALYZE_STRUCT(type) analyze_struct(#type, sizeof(type), alignof(type))

void analyze_struct(const char* name, size_t size, size_t alignment)
{
    printf("\nSTRUCT ANALYSIS: %s\n", name);

    printf("  size             : %zu bytes\n", size);
    printf("  alignment        : %zu bytes\n", alignment);

    size_t cachelines = (size + CACHELINE_SIZE - 1) / CACHELINE_SIZE;

    printf("  cachelines used  : %zu\n", cachelines);

    size_t wasted = cachelines * CACHELINE_SIZE - size;

    printf("  wasted space     : %zu bytes\n", wasted);

    // performance warnings

    if(size < CACHELINE_SIZE)
    {
        printf("  perf             : GOOD (fits in one cacheline)\n");
    }
    else if(size == CACHELINE_SIZE)
    {
        printf("  perf             : PERFECT (exact cacheline fit)\n");
    }
    else
    {
        printf("  perf             : WARNING (spans multiple cachelines)\n");
    }

    if(alignment < 16)
    {
        printf("  alignment warn   : BAD (should be >= 16)\n");
    }
    else if(alignment >= CACHELINE_SIZE)
    {
        printf("  alignment        : EXCELLENT (cacheline aligned)\n");
    }

    if(size % alignment != 0)
    {
        printf("  layout warn      : size not multiple of alignment\n");
    }

    // false sharing risk
    if(size < CACHELINE_SIZE)
    {
        printf("  false sharing    : POSSIBLE\n");
    }
    else
    {
        printf("  false sharing    : SAFE\n");
    }
}


// Renderer
//     ├── InstanceContext
//     ├── PhysicalDeviceInfo
//     ├── DeviceContext
//     ├── SwapchainManager
//     ├── DescriptorAllocator (global)
//     ├── DescriptorAllocator (per-frame)
//     ├── PipelineCacheManager


//so here is a cool pipeline storing idea
//
//     typedef enum PipelineID
//     {
//     PIPELINE_TRIANGLE,
//     PIPELINE_GBUFFER,
//     PIPELINE_LIGHTING,
//     PIPELINE_COMPOSITE,
//
//     PIPELINE_COUNT
// } PipelineID;
// Store:
//
// typedef struct Renderer
// {
//     VkPipeline pipelines[PIPELINE_COUNT];
// } Renderer;
// Use:
//
// renderer.pipelines[PIPELINE_TRIANGLE]              =                 create_graphics_pipeline(...);
//
// vkCmdBindPipeline(cmd,
//     VK_PIPELINE_BIND_POINT_GRAPHICS,
//     renderer.pipelines[PIPELINE_TRIANGLE]);
//
//
//<F9>
//

//i think we can only make one single indless gloal desc layout so we have to ind only that and we are free from managing desc per oject
// but because of different needs of push constants we might or might not maintain just one global pipeline layout
//    Bindful is object-oriented thinking. “Each draw has its state.”
//  Bindless is data-oriented thinking. “All resources live in big tables.”
//
//

// TODO: we need to create a paastype structure with tagged unions which switches between typeofcammand it executes
//
//
//\


typedef enum PipelineID
{
    TRIANGLE_PIPELINE,
    PIPELINE_COUNT
} PipelineID;
typedef struct RenderPipelines
{
    VkPipeline pipelines[PIPELINE_COUNT];
} RendererPipelines;


typedef enum
{

    GRAPHICS,
    COMPUTE,

} PassType;

//TODO:
typedef enum DrawType
{
    DRAW_DIRECT,
    DRAW_INDIRECT,
    DRAW_INDEXED,
    DRAW_INDEXED_INDIRECT,
    DRAW_INDEXED_INDIRECT_COUNT,
} DrawType;
typedef struct DrawCmd
{
    DrawType type;

    union
    {
        struct
        {
            uint32_t vertexCount;
            uint32_t instanceCount;
            uint32_t firstVertex;
            uint32_t firstInstance;
        } direct;

        struct
        {
            VkBuffer     buffer;
            VkDeviceSize offset;
            uint32_t     drawCount;
            uint32_t     stride;
        } indirect;

        struct
        {
            uint32_t indexCount;
            uint32_t instanceCount;
            uint32_t firstIndex;
            int32_t  vertexOffset;
            uint32_t firstInstance;
        } indexed;

        struct
        {
            VkBuffer     buffer;
            VkDeviceSize offset;
            uint32_t     drawCount;
            uint32_t     stride;
        } indexed_indirect;

        struct
        {
            VkBuffer     buffer;
            VkDeviceSize offset;
            VkBuffer     countBuffer;
            VkDeviceSize countOffset;
            uint32_t     maxDrawCount;
            uint32_t     stride;
        } indexed_indirect_count;
    };
} DrawCmd;






typedef enum
{
    RESOURCE_TEXTURE,
    RESOURCE_BUFFER,
} ResourceType;

typedef enum
{
    ACCESS_READ,
    ACCESS_WRITE,
    ACCESS_READ_WRITE,
} ResourceAccess;

typedef enum
{
    USAGE_COLOR_ATTACHMENT,
    USAGE_DEPTH_ATTACHMENT,
    USAGE_SAMPLED,
    USAGE_STORAGE,
    USAGE_TRANSFER_SRC,
    USAGE_TRANSFER_DST,
    USAGE_UNIFORM_BUFFER,
    USAGE_STORAGE_BUFFER,
} ResourceUsage;

typedef struct
{
    ResourceType type;
    union
    {
        TextureHandle texture_handle;
	BufferHandle buffer_handle;
    };
    ResourceUsage  usage;
    ResourceAccess access;
} PassResource;



typedef struct {
    TextureHandle color_attachments[8];
    uint32_t color_count;
    TextureHandle depth_attachment;
} PassAttachments;

typedef struct Pass
{
    PassType   type;
    PipelineID pipeline_id;
    void*      push_data;
    uint32_t   push_size;

    union
    {

        struct
        {
            VkRenderingInfo    rendering;
            VkShaderStageFlags push_stages;  //only in case of gfx may be
            uint32_t           draw_count;
            DrawCmd*           draws;
        } graphics;

        struct
        {
            uint32_t groupX;
            uint32_t groupY;
            uint32_t groupZ;
        } compute;
    };
} Pass;

void execute_pass(Renderer* r, RendererPipelines* pipelines, VkCommandBuffer cmd, Pass* pass)
{
    VkPipeline pipeline = pipelines->pipelines[pass->pipeline_id];

    switch(pass->type)
    {
        case GRAPHICS: {
            vkCmdBeginRendering(cmd, &pass->graphics.rendering);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            vk_cmd_set_viewport_scissor(cmd, r->swapchain.extent);
            vkCmdPushConstants(cmd, r->bindless_system.pipeline_layout, pass->graphics.push_stages, 0, pass->push_size,
                               pass->push_data);


            for(uint32_t i = 0; i < pass->graphics.draw_count; i++)
            {
                DrawCmd* d = &pass->graphics.draws[i];

                switch(d->type)
                {
                    case DRAW_DIRECT:
                        vkCmdDraw(cmd, d->direct.vertexCount, d->direct.instanceCount, d->direct.firstVertex, d->direct.firstInstance);
                        break;

                    case DRAW_INDIRECT:
                        vkCmdDrawIndirect(cmd, d->indirect.buffer, d->indirect.offset, d->indirect.drawCount,
                                          d->indirect.stride);
                        break;

                    case DRAW_INDEXED:
                        vkCmdDrawIndexed(cmd, d->indexed.indexCount, d->indexed.instanceCount, d->indexed.firstIndex,
                                         d->indexed.vertexOffset, d->indexed.firstInstance);
                        break;

                    case DRAW_INDEXED_INDIRECT:
                        vkCmdDrawIndexedIndirect(cmd, d->indexed_indirect.buffer, d->indexed_indirect.offset,
                                                 d->indexed_indirect.drawCount, d->indexed_indirect.stride);
                        break;

                    case DRAW_INDEXED_INDIRECT_COUNT:
                        vkCmdDrawIndexedIndirectCount(cmd, d->indexed_indirect_count.buffer, d->indexed_indirect_count.offset,
                                                      d->indexed_indirect_count.countBuffer, d->indexed_indirect_count.countOffset,
                                                      d->indexed_indirect_count.maxDrawCount, d->indexed_indirect_count.stride);
                        break;
                }
            }

            vkCmdEndRendering(cmd);
        }
        break;

        case COMPUTE: {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

            vkCmdPushConstants(cmd, r->bindless_system.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pass->push_size,
                               pass->push_data);

            vkCmdDispatch(cmd, pass->compute.groupX, pass->compute.groupY, pass->compute.groupZ);
        }
        break;
    }
}
typedef enum PassID
{
    TRIANGLE_PASS,
    PASS_COUNT
} PassID;
typedef struct RenderPasses
{
    Pass Passes[PASS_COUNT];
} RendererPasses;

#define VALIDATION true
static bool g_framebuffer_resized = false;


int main()
{
    // ============================================================
    // Instance / Device setup
    // ============================================================
    VK_CHECK(volkInitialize());

    if(!is_instance_extension_supported("VK_KHR_wayland_surface"))
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    else
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);


    glfwInit();
    const char* dev_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    u32          glfw_ext_count = 0;
    const char** glfw_exts      = glfwGetRequiredInstanceExtensions(&glfw_ext_count);


    RendererDesc desc = {
        .app_name            = "My Renderer",
        .instance_layers     = NULL,
        .instance_extensions = glfw_exts,
        .device_extensions   = dev_exts,

        .instance_layer_count        = 0,
        .instance_extension_count    = glfw_ext_count,
        .device_extension_count      = 1,
        .enable_gpu_based_validation = VALIDATION,
        .enable_validation           = VALIDATION,

        .validation_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .validation_types = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .width  = 800,
        .height = 600,
        //	.swapchain_preferred_presest_mode = VK_PRESENT_MODE_IMMEDIATE_KHR,
        .swapchain_preferred_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .swapchain_preferred_format      = VK_FORMAT_B8G8R8A8_UNORM,
        .swapchain_extra_usage_flags     = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .vsync                           = false,
    };
    Renderer          renderer         = {0};
    RendererPipelines render_pipelines = {0};
    renderer_create(&renderer, &desc);


    GraphicsPipelineConfig cfg                    = pipeline_config_default();
    cfg.vert_path                                 = "compiledshaders/triangle.vert.spv";
    cfg.frag_path                                 = "compiledshaders/triangle.frag.spv";
    cfg.color_attachment_count                    = 1;
    cfg.color_formats                             = &renderer.swapchain.format;
    render_pipelines.pipelines[TRIANGLE_PIPELINE] = create_graphics_pipeline(&renderer, &cfg);


    while(!glfwWindowShouldClose(renderer.window))
    {
        glfwPollEvents();
        int fb_w, fb_h;
        glfwGetFramebufferSize(renderer.window, &fb_w, &fb_h);
        renderer.swapchain.needs_recreate |= g_framebuffer_resized || fb_w != (int)renderer.swapchain.extent.width
                                             || fb_h != (int)renderer.swapchain.extent.height;

        // Window minimized → suspend rendering
        if(fb_w == 0 || fb_h == 0)
        {
            glfwWaitEvents();
            continue;
        }
        if(renderer.swapchain.needs_recreate)
        {
            vkDeviceWaitIdle(renderer.device);
            vk_swapchain_recreate(renderer.device, renderer.physical_device, &renderer.swapchain, fb_w, fb_h,
                                  renderer.graphics_queue, renderer.one_time_gfx_pool);
            g_framebuffer_resized             = false;
            renderer.swapchain.needs_recreate = false;
            continue;  // restart frame cleanly
        }
        {
            vkWaitForFences(renderer.device, 1, &renderer.frames[renderer.current_frame].in_flight_fence, VK_TRUE, UINT64_MAX);
            vkResetFences(renderer.device, 1, &renderer.frames[renderer.current_frame].in_flight_fence);

            /* reset EVERYTHING allocated for this frame */
            vkResetCommandPool(renderer.device, renderer.frames[renderer.current_frame].cmdbufpool, 0);
            vk_swapchain_acquire(renderer.device, &renderer.swapchain,
                                 renderer.frames[renderer.current_frame].image_available_semaphore, VK_NULL_HANDLE, UINT64_MAX);
        }


        vk_cmd_begin(renderer.frames[renderer.current_frame].cmdbuf, true);
        vkCmdBindDescriptorSets(renderer.frames[renderer.current_frame].cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                renderer.bindless_system.pipeline_layout, 0, 1, &renderer.bindless_system.set, 0, NULL);
        vkCmdBindDescriptorSets(renderer.frames[renderer.current_frame].cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                renderer.bindless_system.pipeline_layout, 0, 1, &renderer.bindless_system.set, 0, NULL);
        image_transition_swapchain(renderer.frames[renderer.current_frame].cmdbuf, &renderer.swapchain,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);


        VkRenderingAttachmentInfo color = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                           .imageView = renderer.swapchain.image_views[renderer.swapchain.current_image],
                                           .imageLayout = renderer.swapchain.states[renderer.swapchain.current_image].layout,
                                           .loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                           .storeOp          = VK_ATTACHMENT_STORE_OP_STORE,
                                           .clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}}};

        VkRenderingInfo rendering = {.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                     .renderArea.extent    = renderer.swapchain.extent,
                                     .layerCount           = 1,
                                     .colorAttachmentCount = 1,
                                     .pColorAttachments    = &color};

        vkCmdBeginRendering(renderer.frames[renderer.current_frame].cmdbuf, &rendering);
        vkCmdBindPipeline(renderer.frames[renderer.current_frame].cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          render_pipelines.pipelines[TRIANGLE_PIPELINE]);
        vk_cmd_set_viewport_scissor(renderer.frames[renderer.current_frame].cmdbuf, renderer.swapchain.extent);
        vkCmdDraw(renderer.frames[renderer.current_frame].cmdbuf, 3, 1, 0, 0);
        vkCmdEndRendering(renderer.frames[renderer.current_frame].cmdbuf);

        image_transition_swapchain(renderer.frames[renderer.current_frame].cmdbuf, &renderer.swapchain,
                                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0);
        vk_cmd_end(renderer.frames[renderer.current_frame].cmdbuf);

        VkCommandBufferSubmitInfo cmd_info = {
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = renderer.frames[renderer.current_frame].cmdbuf,
        };
        VkSemaphoreSubmitInfo wait_info = {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore   = renderer.frames[renderer.current_frame].image_available_semaphore,
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0,
        };

        VkSemaphoreSubmitInfo signal_info = {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore   = renderer.swapchain.render_finished[renderer.swapchain.current_image],
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .deviceIndex = 0,
        };

        VkSubmitInfo2 submit = {
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount   = 1,
            .pWaitSemaphoreInfos      = &wait_info,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &cmd_info,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signal_info,
        };

        VK_CHECK(vkQueueSubmit2(renderer.graphics_queue, 1, &submit, renderer.frames[renderer.current_frame].in_flight_fence));
        vk_swapchain_present(renderer.present_queue, &renderer.swapchain,
                             &renderer.swapchain.render_finished[renderer.swapchain.current_image], 1);
        renderer.current_frame = (renderer.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }


    //    ANALYZE_STRUCT(ImageState);
    //renderer_destroy(&renderer);
    return 0;
}
