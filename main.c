#include "tinytypes.h"
#include "vk_default.h"
#include "vk.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stdio.h>
#include <vulkan/vulkan_core.h>

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
// record_frame()
// {
//     begin_rendering();
//     bind_pipeline(TRIANGLE_PIPELINE);
//     push_constants();
//     draw();
//     end_rendering();
// }
//
typedef struct Buffer
{
    VkBuffer        buffer;  // vulkan buffer
    VkDeviceSize    buffer_size;
    VkDeviceAddress address;     // addr of the buffer in the shader
    uint8_t*        mapping;     //this is a CPU pointer directly into GPU-visible memory.
    VmaAllocation   allocation;  // Memory associated with the buffer
} Buffer;


typedef enum PipelineID
{
    TRIANGLE_PIPELINE,
    PIPELINE_COUNT
} PipelineID;
typedef struct RenderPipelines
{
    VkPipeline pipelines[PIPELINE_COUNT];
} RendererPipelines;


#define MAX_PUSH_CONSTANTS 128

typedef enum PassType
{
    PASS_GRAPHICS,
    PASS_COMPUTE,
} PassType;

typedef enum DrawType
{
    DRAW_DIRECT,
    DRAW_INDIRECT,
    DRAW_INDEXED,
    DRAW_INDEXED_INDIRECT,
    DRAW_INDEXED_INDIRECT_COUNT,
} DrawType;

typedef enum DispatchType
{
    DISPATCH_DIRECT,
    DISPATCH_INDIRECT,
} DispatchType;


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

            uint32_t maxDrawCount;
            uint32_t stride;
        } indexed_indirect_count;
    };

} DrawCmd;


typedef struct Pass
{
    PassType type;

    uint32_t pipeline_id;

    VkShaderStageFlags push_stages;
    uint32_t           push_size;

    uint8_t push_data[MAX_PUSH_CONSTANTS];

    union
    {
        struct
        {
            VkRenderingInfo* rendering;
            uint32_t         draw_count;
            DrawCmd*         draws;
        } graphics;
        struct
        {
            DispatchType type;
            union
            {
                struct
                {
                    uint32_t x;
                    uint32_t y;
                    uint32_t z;
                } direct;

                struct
                {
                    VkBuffer     buffer;
                    VkDeviceSize offset;
                } indirect;
            };
        } compute;
    };
} Pass;
static void execute_pass(Renderer* r, RendererPipelines* pipelines, VkCommandBuffer cmd, const Pass* pass)
{
    VkPipeline pipeline = pipelines->pipelines[pass->pipeline_id];

    if(pass->type == PASS_GRAPHICS)
    {
        // Begin rendering
        vkCmdBeginRendering(cmd, pass->graphics.rendering);

        // Bind pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vk_cmd_set_viewport_scissor(cmd, r->swapchain.extent);
        // Push constants
        vkCmdPushConstants(cmd, r->bindless_system.pipeline_layout, pass->push_stages, 0, pass->push_size, pass->push_data);

        // Execute draw commands
        for(uint32_t i = 0; i < pass->graphics.draw_count; i++)
        {
            const DrawCmd* d = &pass->graphics.draws[i];

            switch(d->type)
            {
                case DRAW_DIRECT: {
                    vkCmdDraw(cmd, d->direct.vertexCount, d->direct.instanceCount, d->direct.firstVertex, d->direct.firstInstance);
                }
                break;

                case DRAW_INDIRECT: {
                    vkCmdDrawIndirect(cmd, d->indirect.buffer, d->indirect.offset, d->indirect.drawCount, d->indirect.stride);
                }
                break;

                case DRAW_INDEXED: {
                    vkCmdDrawIndexed(cmd, d->indexed.indexCount, d->indexed.instanceCount, d->indexed.firstIndex,
                                     d->indexed.vertexOffset, d->indexed.firstInstance);
                }
                break;

                case DRAW_INDEXED_INDIRECT: {
                    vkCmdDrawIndexedIndirect(cmd, d->indexed_indirect.buffer, d->indexed_indirect.offset,
                                             d->indexed_indirect.drawCount, d->indexed_indirect.stride);
                }
                break;

                case DRAW_INDEXED_INDIRECT_COUNT: {
                    vkCmdDrawIndexedIndirectCount(cmd, d->indexed_indirect_count.buffer, d->indexed_indirect_count.offset,
                                                  d->indexed_indirect_count.countBuffer, d->indexed_indirect_count.countOffset,
                                                  d->indexed_indirect_count.maxDrawCount, d->indexed_indirect_count.stride);
                }
                break;
            }
        }

        vkCmdEndRendering(cmd);
    }
    else  // PASS_COMPUTE
    {
        // Bind pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        // Push constants
        vkCmdPushConstants(cmd, r->bindless_system.pipeline_layout, pass->push_stages, 0, pass->push_size, pass->push_data);

        // Dispatch
        if(pass->compute.type == DISPATCH_DIRECT)
        {
            vkCmdDispatch(cmd, pass->compute.direct.x, pass->compute.direct.y, pass->compute.direct.z);
        }
        else
        {
            vkCmdDispatchIndirect(cmd, pass->compute.indirect.buffer, pass->compute.indirect.offset);
        }
    }
}

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

    // To enable Debug Printf:
    // 1. Set .enable_debug_printf             = true in RendererDesc
    // 2. In your shader, add: #extension GL_EXT_debug_printf : enable (GLSL)
    //    or just use printf() in HLSL/Slang
    // 3. Add debugPrintfEXT("My value: %f", myvar) in GLSL
    //    or printf("My value: %f", myvar) in HLSL/Slang
    // 4. View output in RenderDoc or Validation Layers

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
        .enable_debug_printf             = false,  // Enable shader debug printf
    };
    Renderer          renderer         = {0};
    RendererPipelines render_pipelines = {0};
    renderer_create(&renderer, &desc);


    GraphicsPipelineConfig cfg                    = pipeline_config_default();
    cfg.vert_path                                 = "compiledshaders/triangle.vert.spv";
    cfg.frag_path                                 = "compiledshaders/triangle.frag.spv";
    cfg.color_attachment_count                    = 1;
    cfg.color_formats                             = &renderer.swapchain.format;
    cfg.use_vertex_input                          = false;
    cfg.depth_test_enable                         = false;
    cfg.depth_write_enable                        = false;
    render_pipelines.pipelines[TRIANGLE_PIPELINE] = create_graphics_pipeline(&renderer, &cfg);
    typedef struct
    {
        float pos[2];
        float color[3];
    } Vertex;

    BufferPool pool = {0};
    buffer_pool_init(&renderer, &pool, 16 * 1024 * 1024,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                     VMA_MEMORY_USAGE_AUTO,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 2048);

    const uint32_t grid_x     = 128;
    const uint32_t grid_y     = 72;
    const uint32_t tri_count  = grid_x * grid_y;
    const uint32_t vert_count = tri_count * 3;
    const uint32_t draw_count = 1;

    BufferSlice vertex_slice   = buffer_pool_alloc(&pool, sizeof(Vertex) * vert_count, 16);
    BufferSlice indirect_slice = buffer_pool_alloc(&pool, sizeof(VkDrawIndirectCommand) * draw_count, 16);
    BufferSlice count_slice    = buffer_pool_alloc(&pool, sizeof(uint32_t), 4);

    Vertex*                cpu_vertices = (Vertex*)vertex_slice.mapped;
    VkDrawIndirectCommand* cpu_indirect = (VkDrawIndirectCommand*)indirect_slice.mapped;
    uint32_t*              cpu_count    = (uint32_t*)count_slice.mapped;

    uint32_t index  = 0;
    float    cell_w = 2.0f / (float)grid_x;
    float    cell_h = 2.0f / (float)grid_y;
    float    tri_w  = cell_w * 0.9f;
    float    tri_h  = cell_h * 0.9f;
    for(uint32_t y = 0; y < grid_y; y++)
        for(uint32_t x = 0; x < grid_x; x++)
        {
            float base_x = -1.0f + (float)x * cell_w;
            float base_y = -1.0f + (float)y * cell_h;

            cpu_vertices[index++] = (Vertex){{base_x, base_y}, {1, 0, 0}};
            cpu_vertices[index++] = (Vertex){{base_x + tri_w, base_y}, {0, 1, 0}};
            cpu_vertices[index++] = (Vertex){{base_x, base_y + tri_h}, {0, 0, 1}};
        }

    cpu_indirect[0] = (VkDrawIndirectCommand){
        .vertexCount   = 3,
        .instanceCount = tri_count,
        .firstVertex   = 0,
        .firstInstance = 0,
    };

    *cpu_count = draw_count;

    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, vertex_slice.offset, vertex_slice.size);
    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, indirect_slice.offset, indirect_slice.size);
    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, count_slice.offset, count_slice.size);

    VkBufferDeviceAddressInfo addrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = pool.buffer};
    VkDeviceAddress           gpu_address = vkGetBufferDeviceAddress(renderer.device, &addrInfo) + vertex_slice.offset;


    typedef struct
    {
        VkDeviceAddress vertex_ptr;
        uint8_t         padding[256 - sizeof(VkDeviceAddress)];
    } Push;
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

        VkCommandBuffer cmd = renderer.frames[renderer.current_frame].cmdbuf;
        vk_cmd_begin(renderer.frames[renderer.current_frame].cmdbuf, false);
        {

            vkCmdBindDescriptorSets(renderer.frames[renderer.current_frame].cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    renderer.bindless_system.pipeline_layout, 0, 1, &renderer.bindless_system.set, 0, NULL);
            vkCmdBindDescriptorSets(renderer.frames[renderer.current_frame].cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    renderer.bindless_system.pipeline_layout, 0, 1, &renderer.bindless_system.set, 0, NULL);
            image_transition_swapchain(renderer.frames[renderer.current_frame].cmdbuf, &renderer.swapchain,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        }

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

        {
            vkCmdBeginRendering(cmd, &rendering);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipelines.pipelines[TRIANGLE_PIPELINE]);
            vk_cmd_set_viewport_scissor(cmd, renderer.swapchain.extent);
            Push push = {gpu_address, {0}};
            vkCmdPushConstants(cmd, renderer.bindless_system.pipeline_layout, VK_SHADER_STAGE_ALL, 0, sizeof(Push), &push);
            vkCmdDrawIndirectCount(cmd, indirect_slice.buffer, indirect_slice.offset, count_slice.buffer,
                                   count_slice.offset, draw_count, sizeof(VkDrawIndirectCommand));
            vkCmdEndRendering(cmd);
        }

        image_transition_swapchain(renderer.frames[renderer.current_frame].cmdbuf, &renderer.swapchain,
                                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 0);
        vk_cmd_end(renderer.frames[renderer.current_frame].cmdbuf);

        {
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
        }
        renderer.current_frame = (renderer.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    printf("%zu", sizeof(BufferSlice));
    //    ANALYZE_STRUCT(ImageState);
    //renderer_destroy(&renderer);
    return 0;
}
