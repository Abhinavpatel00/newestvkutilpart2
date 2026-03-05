
#include "tinytypes.h"
#include "vk_default.h"
#include "vk.h"
#include <GLFW/glfw3.h>
#include <stdalign.h>
#include <stddef.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <vulkan/vulkan_core.h>

#define KB(x) ((x) * 1024ULL)
#define MB(x) ((x) * 1024ULL * 1024ULL)
#define GB(x) ((x) * 1024ULL * 1024ULL * 1024ULL)
#define PAD(name, size) uint8_t name[(size)]
typedef struct
{
    float pos[3];
    float uv[2];
} Vertex;
// we can get texture id in specific range so that it can be encoded in bitfield using flow id in range
// whatare these GPU uniform block
// Cluster grid
// Culling results buffer
// light buffer
// camera state and input state in renderer may be and upload to uniform
//   Light culling buffers
//Scene lifetime state:
// • Scene graph
// • BVH
// • Static meshes
// • GPUBufferArena
//• LightSystem
//
//    Frame lifetime state:
// • Camera matrices
// • Frustum
// • Visible object list
// • Command buffers
// Renderer lifetime state:
// • GPU infrastructure
// • Resource caches and pools
// • Frame contexts
typedef enum PipelineID
{
    TRIANGLE_PIPELINE,
    PIPELINE_COUNT
} PipelineID;
typedef struct RenderPipelines
{
    VkPipeline pipelines[PIPELINE_COUNT];
} RendererPipelines;


// Entity = ID
// Component = data
// System = logic
static float noise2d(int x, int z)
{
    uint32_t n = x * 73856093 ^ z * 19349663;
    n          = (n << 13) ^ n;
    return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
}
#define VALIDATION false 
int main()
{
    VK_CHECK(volkInitialize());
    if(!is_instance_extension_supported("VK_KHR_wayland_surface"))
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    else
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    glfwInit();
    const char* dev_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME};

    u32               glfw_ext_count   = 0;
    const char**      glfw_exts        = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
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
            .device_extension_count      = 2,
            .enable_gpu_based_validation = VALIDATION,
            .enable_validation           = VALIDATION,

            .validation_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .validation_types = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .width  = 1362,
            .height = 749,

            .swapchain_preferred_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            .swapchain_preferred_format      = VK_FORMAT_B8G8R8A8_UNORM,
            .swapchain_extra_usage_flags     = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .vsync                           = false,
            .enable_debug_printf             = false,  // Enable shader debug printf

            .bindless_sampled_image_count     = 65536,
            .bindless_sampler_count           = 256,
            .bindless_storage_image_count     = 16384,
            .enable_pipeline_stats            = false,
            .swapchain_preferred_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR,

        };


        renderer_create(&renderer, &desc);

        GraphicsPipelineConfig cfg                    = pipeline_config_default();
        cfg.vert_path                                 = "compiledshaders/triangle.vert.spv";
        cfg.frag_path                                 = "compiledshaders/triangle.frag.spv";
        cfg.color_attachment_count                    = 1;
        cfg.depth_format                              = renderer.depth[1].format;
        cfg.color_formats                             = &renderer.swapchain.format;
        cfg.polygon_mode                              = VK_POLYGON_MODE_FILL;
        render_pipelines.pipelines[TRIANGLE_PIPELINE] = create_graphics_pipeline(&renderer, &cfg);
    }


    BufferPool pool = {0};
    buffer_pool_init(&renderer, &pool, MB(16),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                     VMA_MEMORY_USAGE_AUTO,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 2048);

    VkBufferDeviceAddressInfo addrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = pool.buffer};
    TextureID tex_id = load_texture(&renderer, "/home/lk/myprojects/flowgame/data/PNG/Tiles/brick_red.png");

    SamplerCreateDesc desc = {.mag_filter = VK_FILTER_LINEAR,
                              .min_filter = VK_FILTER_LINEAR,

                              .address_u   = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .address_v   = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .address_w   = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                              .max_lod     = 1.0f};

    SamplerID linear_sampler = create_sampler(&renderer, &desc);


    BufferSlice vertex_slice   = buffer_pool_alloc(&pool, sizeof(Vertex) * 36, 16);
    BufferSlice indirect_slice = buffer_pool_alloc(&pool, sizeof(VkDrawIndirectCommand), 16);
    BufferSlice count_slice    = buffer_pool_alloc(&pool, sizeof(uint32_t), 4);

    Vertex*                cpu_vertices = (Vertex*)vertex_slice.mapped;
    VkDrawIndirectCommand* cpu_indirect = (VkDrawIndirectCommand*)indirect_slice.mapped;
    uint32_t*              cpu_count    = (uint32_t*)count_slice.mapped;

    /* cube with UVs */
    Vertex cube_vertices[] = {// Back face
                              {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}},
                              {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
                              {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}},
                              {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
                              {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}},
                              {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}},
                              // Front face
                              {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}},
                              {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f}},
                              {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}},
                              {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}},
                              {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}},
                              {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}},
                              // Left face
                              {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
                              {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
                              {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}},
                              {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}},
                              {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}},
                              {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
                              // Right face
                              {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
                              {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}},
                              {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
                              {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}},
                              {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
                              {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}},
                              // Bottom face
                              {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}},
                              {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}},
                              {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f}},
                              {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f}},
                              {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}},
                              {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}},
                              // Top face
                              {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}},
                              {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}},
                              {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
                              {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f}},
                              {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
                              {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}}};
    memcpy(cpu_vertices, cube_vertices, sizeof(cube_vertices));

    /* indirect draw command */

    cpu_indirect[0].vertexCount   = 36;
    cpu_indirect[0].instanceCount = 1;
    cpu_indirect[0].firstVertex   = 0;
    cpu_indirect[0].firstInstance = 0;

    *cpu_count = 1;

    /* flush */

    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, vertex_slice.offset, sizeof(Vertex) * 36);

    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, indirect_slice.offset, sizeof(VkDrawIndirectCommand));

    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, count_slice.offset, sizeof(uint32_t));

    /* device address */


    Camera cam = {
        .position   = {16.0f, 12.0f, -20.0f},
        .yaw        = glm_rad(180.0f),
        .pitch      = glm_rad(-15.0f),
        .move_speed = 3.0f,
        .look_speed = 0.0025f,
        .fov_y      = glm_rad(75.0f),
        .near_z     = 0.05f,
        .far_z      = 2000.0f,

        .view_proj = GLM_MAT4_IDENTITY_INIT,
    };

    glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    typedef struct
    {
        float x, y, z, w;
    } Voxel;

#define GRID_X 32
#define GRID_Y 16
#define GRID_Z 32

    Voxel    voxels[GRID_X * GRID_Y * GRID_Z];
    uint32_t voxel_count = 0;

    for(int z = 0; z < GRID_Z; z++)
    {
        for(int y = 0; y < GRID_Y; y++)
        {
            for(int x = 0; x < GRID_X; x++)
            {
                voxels[voxel_count++] = (Voxel){(float)x, (float)y, (float)z, 0.0f};
            }
        }
    }


    BufferSlice voxel_slice       = buffer_pool_alloc(&pool, sizeof(Voxel) * voxel_count, 16);
    cpu_indirect[0].instanceCount = voxel_count;
    Voxel* cpu_voxels             = (Voxel*)voxel_slice.mapped;
    memcpy(cpu_voxels, voxels, sizeof(Voxel) * voxel_count);

    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, indirect_slice.offset, sizeof(VkDrawIndirectCommand));
    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, voxel_slice.offset, sizeof(Voxel) * voxel_count);

    VkDeviceAddress voxel_ptr = vkGetBufferDeviceAddress(renderer.device, &addrInfo) + voxel_slice.offset;

    VkDeviceAddress vertex_address = vkGetBufferDeviceAddress(renderer.device, &addrInfo) + vertex_slice.offset;

    /* view_proj needs 16-byte alignment (std430 rule for float4x4).
       Offsets: vertex_ptr=0(8), voxel_ptr=8(8), aspect=16(4), voxel_count=20(4),
       _pad=24(8), view_proj=32(64), texture_id=96(4), sampler_id=100(4) */
    PUSH_CONSTANT(Push, VkDeviceAddress vertex_ptr; VkDeviceAddress voxel_ptr; float aspect; uint32_t voxel_count;
                  uint32_t _align_pad[2]; float view_proj[4][4]; uint32_t texture_id; uint32_t sampler_id;);
    while(!glfwWindowShouldClose(renderer.window))
    {

        frame_start(&renderer, &cam);
        VkCommandBuffer cmd        = renderer.frames[renderer.current_frame].cmdbuf;
        GpuProfiler*    frame_prof = &renderer.gpuprofiler[renderer.current_frame];
        vk_cmd_begin(cmd, false);
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.bindless_system.pipeline_layout, 0,
                                    1, &renderer.bindless_system.set, 0, NULL);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, renderer.bindless_system.pipeline_layout, 0, 1,
                                    &renderer.bindless_system.set, 0, NULL);
            image_transition_swapchain(cmd, &renderer.swapchain, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
            rt_transition_all(cmd, &renderer.depth[renderer.swapchain.current_image],
                              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                              VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
        }

        VkRenderingAttachmentInfo color = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                           .imageView = renderer.swapchain.image_views[renderer.swapchain.current_image],
                                           .imageLayout = renderer.swapchain.states[renderer.swapchain.current_image].layout,
                                           .loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                           .storeOp          = VK_ATTACHMENT_STORE_OP_STORE,
                                           .clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}}};


        VkRenderingAttachmentInfo depth = {
            .sType                   = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView               = renderer.depth[renderer.swapchain.current_image].view,
            .imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp                 = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue.depthStencil = {0.0f, 0},
        };

        VkRenderingInfo rendering = {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea.extent    = renderer.swapchain.extent,
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &color,
            .pDepthAttachment     = &depth,
        };


        {
            imgui_begin_frame();


            igBegin("Renderer Debug", NULL, 0);

            double cpu_frame_ms = renderer.cpu_frame_ns / 1000000.0;
            double cpu_active_ms = renderer.cpu_active_ns / 1000000.0;
            double cpu_wait_ms = renderer.cpu_wait_ns / 1000000.0;

            igText("CPU frame (wall): %.3f ms", cpu_frame_ms);
            igText("CPU active: %.3f ms", cpu_active_ms);
            igText("CPU wait: %.3f ms", cpu_wait_ms);
            igText("FPS: %.1f", cpu_frame_ms > 0.0 ? 1000.0 / cpu_frame_ms : 0.0);
            igSeparator();

            igText("Camera Position");
            igText("x: %.3f", cam.position[0]);
            igText("y: %.3f", cam.position[1]);
            igText("z: %.3f", cam.position[2]);

            igSeparator();

            igText("Yaw: %.3f", cam.yaw);
            igText("Pitch: %.3f", cam.pitch);

            igSeparator();
            igText("GPU Profiler");
            if(frame_prof->pass_count == 0)
            {
                igText("No GPU samples collected yet.");
            }
            for(uint32_t i = 0; i < frame_prof->pass_count; i++)
            {
                GpuPass* pass = &frame_prof->passes[i];
                igText("%s: %.3f ms", pass->name, pass->time_ms);
                if(frame_prof->enable_pipeline_stats)
                {
                    igText("  VS: %llu | FS: %llu | Prim: %llu", (unsigned long long)pass->vs_invocations,
                           (unsigned long long)pass->fs_invocations, (unsigned long long)pass->primitives);
                }
            }

            igEnd();
            igRender();
        }
        gpu_profiler_begin_frame(frame_prof, cmd);

        {
            vkCmdBeginRendering(cmd, &rendering);

            GPU_SCOPE(frame_prof, cmd, "Main Pass", VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipelines.pipelines[TRIANGLE_PIPELINE]);

                vk_cmd_set_viewport_scissor(cmd, renderer.swapchain.extent);

                Push push = {0};

                push.vertex_ptr = vertex_address;
                push.aspect     = (float)renderer.swapchain.extent.width / (float)renderer.swapchain.extent.height;

                push.texture_id  = tex_id;
                push.sampler_id  = linear_sampler;
                push.voxel_ptr   = voxel_ptr;
                push.voxel_count = voxel_count;
                glm_mat4_copy(cam.view_proj, push.view_proj);

                vkCmdPushConstants(cmd, renderer.bindless_system.pipeline_layout, VK_SHADER_STAGE_ALL, 0, sizeof(Push), &push);

                vkCmdDrawIndirectCount(cmd, indirect_slice.buffer, indirect_slice.offset, count_slice.buffer,
                                       count_slice.offset, 1, sizeof(VkDrawIndirectCommand));
            }

            {
                ImDrawData* draw_data = igGetDrawData();
                ImGui_ImplVulkan_RenderDrawData(draw_data, cmd, VK_NULL_HANDLE);
            }


            vkCmdEndRendering(cmd);
        }

        image_transition_swapchain(renderer.frames[renderer.current_frame].cmdbuf, &renderer.swapchain,
                                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
        vk_cmd_end(renderer.frames[renderer.current_frame].cmdbuf);


        submit_frame(&renderer);
    }


printf("Push size = %zu\n", sizeof(Push));
printf("view_proj offset = %zu\n", offsetof(Push, view_proj));
    printf(" pushis %zu    ", alignof(Push));
    printf(" renderer size is %zu", sizeof(Renderer));
    //    ANALYZE_STRUCT(ImageState);
    //renderer_destroy(&renderer);
    return 0;
}


// Q: Are ya shipping, son?
// A: I'm building the pipeline that builds the pipeline.
