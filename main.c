
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
static inline uint64_t time_now_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
#define RUN_ONCE for(static int _once = 1; _once; _once = 0)

#define PUSH_CONSTANT(name, BODY)                                                                                      \
    typedef struct ALIGNAS(16) name_init                                                                               \
    {                                                                                                                  \
        BODY                                                                                                           \
    } name_init;                                                                                                       \
    enum                                                                                                               \
    {                                                                                                                  \
        name##_pad_size = 256 - sizeof(name_init)                                                                      \
    };                                                                                                                 \
                                                                                                                       \
    typedef struct ALIGNAS(16) name                                                                                    \
    {                                                                                                                  \
        BODY uint8_t _pad[name##_pad_size];                                                                            \
    } name;                                                                                                            \
                                                                                                                       \
    _Static_assert(sizeof(name) == 256, "Push constant != 256");
typedef struct
{
    float pos[3];
    float uv[2];
} Vertex;

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

void print_device_extensions(VkPhysicalDevice phys)
{
    uint32_t count = 0;

    vkEnumerateDeviceExtensionProperties(phys, NULL, &count, NULL);

    VkExtensionProperties* extensions = malloc(sizeof(VkExtensionProperties) * count);

    vkEnumerateDeviceExtensionProperties(phys, NULL, &count, extensions);

    printf("Device supports %u extensions:\n\n", count);

    for(uint32_t i = 0; i < count; i++)
    {


        printf("%-40s  spec %u\n", extensions[i].extensionName, extensions[i].specVersion);
    }

    free(extensions);
}


#define VALIDATION false
static bool g_framebuffer_resized = false;
int         main()
{
    VK_CHECK(volkInitialize());
    if(!is_instance_extension_supported("VK_KHR_wayland_surface"))
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    else
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    glfwInit();
    const char* dev_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME};

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
            .device_extension_count      = 2,
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
            .enable_pipeline_stats        = true,
        };


        renderer_create(&renderer, &desc);

        GraphicsPipelineConfig cfg                    = pipeline_config_default();
        cfg.vert_path                                 = "compiledshaders/triangle.vert.spv";
        cfg.frag_path                                 = "compiledshaders/triangle.frag.spv";
        cfg.color_attachment_count                    = 1;
        cfg.color_formats                             = &renderer.swapchain.format;
        cfg.use_vertex_input                          = false;
        cfg.polygon_mode                              = VK_POLYGON_MODE_FILL;
        render_pipelines.pipelines[TRIANGLE_PIPELINE] = create_graphics_pipeline(&renderer, &cfg);
    }


    BufferPool pool = {0};
    buffer_pool_init(&renderer, &pool, MB(16),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                     VMA_MEMORY_USAGE_AUTO,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 2048);


    Camera cam = {
        .position   = {0.0f, 0.0f, 3.0f},
        .yaw        = 0.0f,
        .pitch      = 0.0f,
        .move_speed = 3.0f,
        .look_speed = 0.0025f,
        .fov_y      = glm_rad(75.0f),
        .near_z     = 0.05f,
        .far_z      = 2000.0f,
    };

    glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    double last_time        = glfwGetTime();
    mat4   camera_view_proj = GLM_MAT4_IDENTITY_INIT;

    uint64_t      frame_start  = time_now_ns();
    static double cpu_frame_ms = 0.3;
    while(!glfwWindowShouldClose(renderer.window))
    {

        frame_start = time_now_ns();
        glfwPollEvents();

        double now = glfwGetTime();
        float  dt  = (float)(now - last_time);
        last_time  = now;

        int fb_w, fb_h;
        glfwGetFramebufferSize(renderer.window, &fb_w, &fb_h);
        renderer.swapchain.needs_recreate |= g_framebuffer_resized || fb_w != (int)renderer.swapchain.extent.width
                                             || fb_h != (int)renderer.swapchain.extent.height;

        static bool mouse_captured = false;

        static double lastX = 0.0, lastY = 0.0;
        if(glfwGetMouseButton(renderer.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        {
            if(!mouse_captured)
            {
                glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                mouse_captured = true;
                double x, y;
                glfwGetCursorPos(renderer.window, &x, &y);
                lastX = x;
                lastY = y;
            }
        }
        else
        {
            if(mouse_captured)
            {
                glfwSetInputMode(renderer.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                mouse_captured = false;
            }
        }

        {  // camera

            if(mouse_captured)
            {
                static bool firstMouse = true;

                double xpos, ypos;
                glfwGetCursorPos(renderer.window, &xpos, &ypos);

                if(firstMouse)
                {
                    lastX      = xpos;
                    lastY      = ypos;
                    firstMouse = false;
                }

                float dx = (float)(xpos - lastX);
                float dy = (float)(ypos - lastY);

                lastX = xpos;
                lastY = ypos;

                cam.yaw += dx * cam.look_speed;
                cam.pitch -= dy * cam.look_speed;  // invert Y properly

                float limit = glm_rad(89.0f);
                cam.pitch   = glm_clamp(cam.pitch, -limit, limit);
                if(cam.pitch > limit)
                    cam.pitch = limit;
                if(cam.pitch < -limit)
                    cam.pitch = -limit;
            }


            vec3 forward = {
                cosf(cam.pitch) * sinf(cam.yaw),
                sinf(cam.pitch),
                -cosf(cam.pitch) * cosf(cam.yaw),
            };
            glm_vec3_normalize(forward);

            vec3 world_up = {0.0f, 1.0f, 0.0f};
            vec3 right    = {0};
            vec3 up       = {0};
            glm_vec3_cross(forward, world_up, right);
            glm_vec3_normalize(right);
            glm_vec3_cross(right, forward, up);

            float speed = cam.move_speed;
            if(glfwGetKey(renderer.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                speed *= 3.0f;

            vec3 delta = {0};
            if(glfwGetKey(renderer.window, GLFW_KEY_W) == GLFW_PRESS)
                glm_vec3_muladds(forward, speed * dt, delta);
            if(glfwGetKey(renderer.window, GLFW_KEY_S) == GLFW_PRESS)
                glm_vec3_muladds(forward, -speed * dt, delta);
            if(glfwGetKey(renderer.window, GLFW_KEY_D) == GLFW_PRESS)
                glm_vec3_muladds(right, speed * dt, delta);
            if(glfwGetKey(renderer.window, GLFW_KEY_A) == GLFW_PRESS)
                glm_vec3_muladds(right, -speed * dt, delta);
            if(glfwGetKey(renderer.window, GLFW_KEY_E) == GLFW_PRESS)
                glm_vec3_muladds(world_up, speed * dt, delta);
            if(glfwGetKey(renderer.window, GLFW_KEY_Q) == GLFW_PRESS)
                glm_vec3_muladds(world_up, -speed * dt, delta);

            glm_vec3_add(cam.position, delta, cam.position);

            vec3 center = {0};
            glm_vec3_add(cam.position, forward, center);

            mat4 view      = GLM_MAT4_IDENTITY_INIT;
            mat4 proj      = GLM_MAT4_IDENTITY_INIT;
            mat4 view_proj = GLM_MAT4_IDENTITY_INIT;

            glm_lookat(cam.position, center, up, view);


            float aspect = (float)renderer.swapchain.extent.width / (float)renderer.swapchain.extent.height;

            camera_build_proj_reverse_z_infinite(proj, &cam, aspect);
            proj[1][1] *= -1.0f;  // Vulkan Y flip
            glm_mat4_mul(proj, view, view_proj);
            glm_mat4_copy(view_proj, camera_view_proj);
        }

        {
            // frustum
            mat4 m;

            glm_mat4_copy(camera_view_proj, m);

            renderer.frustum.planes[LeftPlane][0] = m[0][3] + m[0][0];
            renderer.frustum.planes[LeftPlane][1] = m[1][3] + m[1][0];
            renderer.frustum.planes[LeftPlane][2] = m[2][3] + m[2][0];
            renderer.frustum.planes[LeftPlane][3] = m[3][3] + m[3][0];

            //RIGHT  : row3 - row0
            renderer.frustum.planes[RightPlane][0] = m[0][3] - m[0][0];
            renderer.frustum.planes[RightPlane][1] = m[1][3] - m[1][0];
            renderer.frustum.planes[RightPlane][2] = m[2][3] - m[2][0];
            renderer.frustum.planes[RightPlane][3] = m[3][3] - m[3][0];

            // row3 + row1
            renderer.frustum.planes[BottomPlane][0] = m[0][3] + m[0][1];
            renderer.frustum.planes[BottomPlane][1] = m[1][3] + m[1][1];
            renderer.frustum.planes[BottomPlane][2] = m[2][3] + m[2][1];
            renderer.frustum.planes[BottomPlane][3] = m[3][3] + m[3][1];

            //TOP   row3 - row1
            renderer.frustum.planes[TopPlane][0] = m[0][3] - m[0][1];
            renderer.frustum.planes[TopPlane][1] = m[1][3] - m[1][1];
            renderer.frustum.planes[TopPlane][2] = m[2][3] - m[2][1];
            renderer.frustum.planes[TopPlane][3] = m[3][3] - m[3][1];

            //   : row3 + row2   (Vulkan style)
            renderer.frustum.planes[NearPlane][0] = m[0][3] + m[0][2];
            renderer.frustum.planes[NearPlane][1] = m[1][3] + m[1][2];
            renderer.frustum.planes[NearPlane][2] = m[2][3] + m[2][2];
            renderer.frustum.planes[NearPlane][3] = m[3][3] + m[3][2];

            //     FAR    : row3 - row2
            renderer.frustum.planes[FarPlane][0] = m[0][3] - m[0][2];
            renderer.frustum.planes[FarPlane][1] = m[1][3] - m[1][2];
            renderer.frustum.planes[FarPlane][2] = m[2][3] - m[2][2];
            renderer.frustum.planes[FarPlane][3] = m[3][3] - m[3][2];

            forEach(i, FrustumPlaneCount)
            {
                vec4* p       = &renderer.frustum.planes[i];
                float len     = sqrtf((*p)[0] * (*p)[0] + (*p)[1] * (*p)[1] + (*p)[2] * (*p)[2]);
                float inv_len = 1.0f / len;
                (*p)[0] *= inv_len;
                (*p)[1] *= inv_len;
                (*p)[2] *= inv_len;
                (*p)[3] *= inv_len;
            }
        }
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
            GpuProfiler* frame_prof = &renderer.gpuprofiler[renderer.current_frame];
            gpu_profiler_collect(frame_prof, renderer.device);

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

        imgui_begin_frame();

        GpuProfiler* frame_prof = &renderer.gpuprofiler[renderer.current_frame];

        igBegin("Renderer Debug", NULL, 0);

        igText("CPU frame: %.3f ms", cpu_frame_ms);
        igText("FPS: %.1f", 1000.0f / cpu_frame_ms);

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

        gpu_profiler_begin_frame(frame_prof, cmd);

        GPU_SCOPE(frame_prof, cmd, "Main Pass", VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT)
        {
            vkCmdBeginRendering(cmd, &rendering);
            static bool initialized = false;

            static TextureID tex_id;
            static SamplerID linear_sampler;

            static BufferSlice vertex_slice;
            static BufferSlice indirect_slice;
            static BufferSlice count_slice;

            static VkDeviceAddress gpu_address;

            PUSH_CONSTANT(Push, VkDeviceAddress vertex_ptr; float aspect; uint32_t _pad0; float view_proj[4][4];
                          uint32_t texture_id; uint32_t sampler_id;);
            if(!initialized)
            {
                tex_id = load_texture(&renderer, "/home/lk/myprojects/flowgame/data/PNG/Tiles/brick_red.png");

                SamplerCreateDesc desc = {.mag_filter = VK_FILTER_LINEAR,
                                          .min_filter = VK_FILTER_LINEAR,

                                          .address_u = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .address_v = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                          .address_w = VK_SAMPLER_ADDRESS_MODE_REPEAT,

                                          .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                          .max_lod     = 1.0f};

                linear_sampler = create_sampler(&renderer, &desc);

                /* allocate buffers */

                vertex_slice   = buffer_pool_alloc(&pool, sizeof(Vertex) * 36, 16);
                indirect_slice = buffer_pool_alloc(&pool, sizeof(VkDrawIndirectCommand), 16);
                count_slice    = buffer_pool_alloc(&pool, sizeof(uint32_t), 4);

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

                VkBufferDeviceAddressInfo addrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = pool.buffer};

                gpu_address = vkGetBufferDeviceAddress(renderer.device, &addrInfo) + vertex_slice.offset;

                initialized = true;
            }


            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipelines.pipelines[TRIANGLE_PIPELINE]);

                vk_cmd_set_viewport_scissor(cmd, renderer.swapchain.extent);

                Push push = {0};

                push.vertex_ptr = gpu_address;
                push.aspect     = (float)renderer.swapchain.extent.width / (float)renderer.swapchain.extent.height;

                push.texture_id = tex_id;
                push.sampler_id = linear_sampler;

                glm_mat4_copy(camera_view_proj, push.view_proj);

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
        uint64_t frame_end     = time_now_ns();

        cpu_frame_ms = (frame_end - frame_start) / 1000000.0;
    }


    printf(" renderer size is %zu", sizeof(Renderer));
    printf(" rt is %zu", sizeof(RenderTarget));
    //    ANALYZE_STRUCT(ImageState);
    //renderer_destroy(&renderer);
    return 0;
}


// Q: Are ya shipping, son?
// A: I'm building the pipeline that builds the pipeline.
