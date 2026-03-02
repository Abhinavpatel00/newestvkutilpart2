#include "tinytypes.h"
#include "vk_default.h"
#include "vk.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <vulkan/vulkan_core.h>

typedef struct Buffer
{
    VkBuffer        buffer;  // vulkan buffer
    VkDeviceSize    buffer_size;
    VkDeviceAddress address;     // addr of the buffer in the shader
    uint8_t*        mapping;     //this is a CPU pointer directly into GPU-visible memory.
    VmaAllocation   allocation;  // Memory associated with the buffer
} Buffer;


typedef struct
{
    float pos[2];
    float color[3];
} Vertex;


typedef enum PipelineID
{
    TRIANGLE_PIPELINE,
    PIPELINE_COUNT
} PipelineID;
typedef struct RenderPipelines
{
    VkPipeline pipelines[PIPELINE_COUNT];
} RendererPipelines;

#define VALIDATION true
static bool g_framebuffer_resized = false;
void        rebuild_geometry(Vertex* cpu_vertices, VkDrawIndirectCommand* cpu_indirect, uint32_t n)
{
    uint32_t poly_vertices = n + 2;
    float    radius        = 0.8f;

    uint32_t index = 0;

    for(uint32_t i = 1; i < poly_vertices - 1; ++i)
    {
        float a0 = 0;
        float a1 = (2.0f * M_PI * i) / poly_vertices;
        float a2 = (2.0f * M_PI * (i + 1)) / poly_vertices;

        cpu_vertices[index++] = (Vertex){{cosf(a0) * radius, sinf(a0) * radius}, {1, 0, 0}};
        cpu_vertices[index++] = (Vertex){{cosf(a1) * radius, sinf(a1) * radius}, {0, 1, 0}};
        cpu_vertices[index++] = (Vertex){{cosf(a2) * radius, sinf(a2) * radius}, {0, 0, 1}};
    }

    cpu_indirect[0].vertexCount = n * 3;
}
int main()
{
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
    // 1. Set .enable_debug_printf             = false in RendererDesc
    // 2. In your shader, add: #extension GL_EXT_debug_printf : enable (GLSL)
    //    or just use printf() in HLSL/Slang
    // 3. Add debugPrintfEXT("My value: %f", myvar) in GLSL
    //    or printf("My value: %f", myvar) in HLSL/Slang
    // 4. View output in RenderDoc or Validation Layers
    Renderer          renderer         = {0};
    RendererPipelines render_pipelines = {0};

    {
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
            .width                           = 1362,
            .height                          = 749,
            .swapchain_preferred_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            .swapchain_preferred_format      = VK_FORMAT_B8G8R8A8_UNORM,
            .swapchain_extra_usage_flags     = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .vsync                           = false,
            .enable_debug_printf             = false,  // Enable shader debug printf

            .bindless_sampled_image_count = 65536,
            .bindless_sampler_count       = 256,
            .bindless_storage_image_count = 16384,

        };


        renderer_create(&renderer, &desc);

        GraphicsPipelineConfig cfg                    = pipeline_config_default();
        cfg.vert_path                                 = "compiledshaders/triangle.vert.spv";
        cfg.frag_path                                 = "compiledshaders/triangle.frag.spv";
        cfg.color_attachment_count                    = 1;
        cfg.color_formats                             = &renderer.swapchain.format;
        cfg.use_vertex_input                          = false;
        cfg.depth_test_enable                         = false;
        cfg.depth_write_enable                        = false;
        cfg.polygon_mode                              = VK_POLYGON_MODE_LINE;
        render_pipelines.pipelines[TRIANGLE_PIPELINE] = create_graphics_pipeline(&renderer, &cfg);
    }
    BufferPool pool = {0};
    buffer_pool_init(&renderer, &pool, 16 * 1024 * 1024,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                     VMA_MEMORY_USAGE_AUTO,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 2048);
    //
    // const uint32_t grid_x     = 128;
    // const uint32_t grid_y     = 72;
    // const uint32_t tri_count  = grid_x * grid_y;
    // const uint32_t vert_count = tri_count * 3;
    // const uint32_t draw_count = 1;
    //
    //
    // uint32_t index  = 0;
    // float    cell_w = 2.0f / (float)grid_x;
    // float    cell_h = 2.0f / (float)grid_y;
    // float    tri_w  = cell_w * 0.9f;
    // float    tri_h  = cell_h * 0.9f;
    // for(uint32_t y = 0; y < grid_y; y++)
    //     for(uint32_t x = 0; x < grid_x; x++)
    //     {
    //         float base_x = -1.0f + (float)x * cell_w;
    //         float base_y = -1.0f + (float)y * cell_h;
    //
    //         cpu_vertices[index++] = (Vertex){{base_x, base_y}, {1, 0, 0}};
    //         cpu_vertices[index++] = (Vertex){{base_x + tri_w, base_y}, {0, 1, 0}};
    //         cpu_vertices[index++] = (Vertex){{base_x, base_y + tri_h}, {0, 0, 1}};
    //     }
    //
    //


    uint32_t       n             = 4222;
    uint32_t       max_triangles = 62202;  // allocate once for worst case
    const uint32_t poly_vertices = n + 2;

    uint32_t       vert_capacity = max_triangles * 3;
    const uint32_t draw_count    = 1;

    BufferSlice vertex_slice   = buffer_pool_alloc(&pool, sizeof(Vertex) * vert_capacity, 16);
    BufferSlice indirect_slice = buffer_pool_alloc(&pool, sizeof(VkDrawIndirectCommand) * draw_count, 16);
    BufferSlice count_slice    = buffer_pool_alloc(&pool, sizeof(uint32_t), 4);

    Vertex*                cpu_vertices = (Vertex*)vertex_slice.mapped;
    VkDrawIndirectCommand* cpu_indirect = (VkDrawIndirectCommand*)indirect_slice.mapped;
    uint32_t*              cpu_count    = (uint32_t*)count_slice.mapped;

    typedef struct
    {
        float x, y;
    } Vec2;

    Vec2* poly = malloc(sizeof(Vec2) * poly_vertices);

    float radius = 0.8f;

    for(uint32_t i = 0; i < poly_vertices; ++i)
    {
        float angle = (2.0f * M_PI * i) / (float)poly_vertices;
        poly[i].x   = cosf(angle) * radius;
        poly[i].y   = sinf(angle) * radius;
    }
    uint32_t index = 0;

    for(uint32_t i = 1; i < poly_vertices - 1; ++i)
    {
        // triangle = (0, i, i+1)

        cpu_vertices[index++] = (Vertex){{poly[0].x, poly[0].y}, {1, 0, 0}};
        cpu_vertices[index++] = (Vertex){{poly[i].x, poly[i].y}, {0, 1, 0}};
        cpu_vertices[index++] = (Vertex){{poly[i + 1].x, poly[i + 1].y}, {0, 0, 1}};
    }


    cpu_indirect[0] = (VkDrawIndirectCommand){
        .vertexCount   = n * 3,
        .instanceCount = 1,
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
        float           aspect;
        uint8_t         padding[256 - sizeof(VkDeviceAddress) - sizeof(float)];
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
            {
                imgui_begin_frame();
                igBegin("Control", NULL, 0);
                if(igButton("+1", (ImVec2){60, 0}))
                {
                    if(n < max_triangles)
                        n++;
                }
                igSameLine(0, 10);
                if(igButton("-1", (ImVec2){60, 0}))
                {
                    if(n > 1)
                        n--;
                }
                igText("Triangles: %u", n);
                igEnd();
                igRender();

                static uint32_t prev_n = 0;

                if(n != prev_n)
                {
                    rebuild_geometry(cpu_vertices, cpu_indirect, n);

                    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, vertex_slice.offset, sizeof(Vertex) * n * 3);

                    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, indirect_slice.offset, sizeof(VkDrawIndirectCommand));

                    prev_n = n;
                }
            }
            ImDrawData* draw_data = igGetDrawData();
            {

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipelines.pipelines[TRIANGLE_PIPELINE]);
                vk_cmd_set_viewport_scissor(cmd, renderer.swapchain.extent);
                //        Push push = {gpu_address, (float)renderer.swapchain.extent.width / (float)renderer.swapchain.extent.height, {0}};

                Push push       = {0};
                push.vertex_ptr = gpu_address;
                push.aspect     = (float)renderer.swapchain.extent.width / (float)renderer.swapchain.extent.height;
                vkCmdPushConstants(cmd, renderer.bindless_system.pipeline_layout, VK_SHADER_STAGE_ALL, 0, sizeof(Push), &push);
                vkCmdDrawIndirectCount(cmd, indirect_slice.buffer, indirect_slice.offset, count_slice.buffer,
                                       count_slice.offset, draw_count, sizeof(VkDrawIndirectCommand));
            }
            {
                ImGui_ImplVulkan_RenderDrawData(draw_data, cmd, VK_NULL_HANDLE);
            }
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
