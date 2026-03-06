
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
#define STB_PERLIN_IMPLEMENTATION
#include "stb/stb_perlin.h"
#define VALIDATION true
#define KB(x) ((x) * 1024ULL)
#define MB(x) ((x) * 1024ULL * 1024ULL)
#define GB(x) ((x) * 1024ULL * 1024ULL * 1024ULL)
#define PAD(name, size) uint8_t name[(size)]
typedef struct
{
    float pos[3];
    float uv[2];
} Vertex;
// todo :different vertex compression helpers sfor every possible kind of stuff like two d and three d vertex with optimal helpers for slang to decompress
// make a single header noise library with slang and c for cpu and gpu

// whatare these GPU uniform block
// Cluster grid
// Culling results buffer
// light buffer
// camera state and input state in renderer may be and upload to uniform
// Light culling buffers
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
    POSTPROCESS_PIPELINE,
    PIPELINE_COUNT
} PipelineID;
typedef struct RenderPipelines
{
    VkPipeline pipelines[PIPELINE_COUNT];
} RendererPipelines;
uint32_t squirrel_noise5(int position, uint32_t seed)
{
    const uint32_t BIT_NOISE1 = 0xd2a80a3f;
    const uint32_t BIT_NOISE2 = 0xa884f197;
    const uint32_t BIT_NOISE3 = 0x6c736f4b;

    uint32_t mangled = position;
    mangled *= BIT_NOISE1;
    mangled += seed;
    mangled ^= (mangled >> 8);
    mangled += BIT_NOISE2;
    mangled ^= (mangled << 8);
    mangled *= BIT_NOISE3;
    mangled ^= (mangled >> 8);

    return mangled;
}
// Entity = ID
// Component = data
// System = logic

typedef struct
{
    uint16_t face_tex[6];
} VoxelMaterial;


enum
{
    TEX_STONE,
    TEX_GRASS_TOP,
    TEX_GRASS_SIDE,
    TEX_DIRT,
    TEX_COUNT
};
typedef enum
{
    FACE_POS_X = 0,
    FACE_NEG_X = 1,
    FACE_POS_Y = 2,
    FACE_NEG_Y = 3,
    FACE_POS_Z = 4,
    FACE_NEG_Z = 5
} FaceDir;
typedef enum
{

    VOXEL_AIR = 0,
    STONE,
    GRASS
} VoxelType;
VoxelMaterial voxel_materials[] = {
    [VOXEL_AIR] = {0, 0, 0, 0, 0, 0},

    [STONE] = {.face_tex =
                   {
                       TEX_STONE,  // +X
                       TEX_STONE,  // -X
                       TEX_STONE,  // +Y
                       TEX_STONE,  // -Y
                       TEX_STONE,  // +Z
                       TEX_STONE   // -Z
                   }},

    [GRASS] = {.face_tex = {TEX_GRASS_SIDE, TEX_GRASS_SIDE, TEX_GRASS_TOP, TEX_DIRT, TEX_GRASS_SIDE, TEX_GRASS_SIDE}}};
typedef struct
{
    uint32_t data0;
    uint32_t data1;
} PackedFace;

typedef struct
{
    VoxelType type;
} Voxel;


#define CHUNK_SIZE 128

typedef struct
{
    Voxel voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
} Chunk;

typedef struct
{
    PackedFace* faces;
    uint32_t    face_count;
} ChunkMesh;

static inline PackedFace pack_face(uint32_t x, uint32_t y, uint32_t z, uint32_t face, uint32_t tex)
{
    PackedFace f;

    f.data0 = (x & 255) | ((y & 255) << 8) | ((z & 4095) << 16) | ((face & 7) << 28);

    f.data1 = tex;

    return f;
}
static inline bool is_air(Chunk* c, int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0)
        return true;
    if(x >= CHUNK_SIZE)
        return true;
    if(y >= CHUNK_SIZE)
        return true;
    if(z >= CHUNK_SIZE)
        return true;

    return c->voxels[x][y][z].type == VOXEL_AIR;
}
void build_chunk_mesh(Chunk* chunk, ChunkMesh* mesh)
{
    mesh->face_count = 0;

    for(int x = 0; x < CHUNK_SIZE; x++)
        for(int y = 0; y < CHUNK_SIZE; y++)
            for(int z = 0; z < CHUNK_SIZE; z++)
            {
                Voxel v = chunk->voxels[x][y][z];

                if(v.type == VOXEL_AIR)
                    continue;

                if(is_air(chunk, x + 1, y, z))
                    mesh->faces[mesh->face_count++] =
                        pack_face(x, y, z, FACE_POS_X, voxel_materials[v.type].face_tex[FACE_POS_X]);

                if(is_air(chunk, x - 1, y, z))
                    mesh->faces[mesh->face_count++] =
                        pack_face(x, y, z, FACE_NEG_X, voxel_materials[v.type].face_tex[FACE_NEG_X]);

                if(is_air(chunk, x, y + 1, z))
                    mesh->faces[mesh->face_count++] =
                        pack_face(x, y, z, FACE_POS_Y, voxel_materials[v.type].face_tex[FACE_POS_Y]);

                if(is_air(chunk, x, y - 1, z))
                    mesh->faces[mesh->face_count++] =
                        pack_face(x, y, z, FACE_NEG_Y, voxel_materials[v.type].face_tex[FACE_NEG_Y]);

                if(is_air(chunk, x, y, z + 1))
                    mesh->faces[mesh->face_count++] =
                        pack_face(x, y, z, FACE_POS_Z, voxel_materials[v.type].face_tex[FACE_POS_Z]);

                if(is_air(chunk, x, y, z - 1))
                    mesh->faces[mesh->face_count++] =
                        pack_face(x, y, z, FACE_NEG_Z, voxel_materials[v.type].face_tex[FACE_NEG_Z]);
            }
}

float terrain(float x, float z)
{
    return stb_perlin_fbm_noise3(x * 0.01f, 0.0f, z * 0.01f,
                                 2.0f,  // lacunarity
                                 0.5f,  // gain
                                 6      // octaves
    );
}
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
            .enable_gpu_based_validation = false,
            .enable_validation           = VALIDATION,

            .validation_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                                   | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .validation_types = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .width  = 1362,
            .height = 749,

            .swapchain_preferred_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            .swapchain_preferred_format      = VK_FORMAT_B8G8R8A8_UNORM,
            .swapchain_extra_usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .vsync               = false,
            .enable_debug_printf = false,  // Enable shader debug printf

            .bindless_sampled_image_count     = 65536,
            .bindless_sampler_count           = 256,
            .bindless_storage_image_count     = 16384,
            .enable_pipeline_stats            = false,
            .swapchain_preferred_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR,

        };


        renderer_create(&renderer, &desc);

        GraphicsPipelineConfig cfg = pipeline_config_default();
        cfg.vert_path              = "compiledshaders/triangle.vert.spv";
        cfg.frag_path              = "compiledshaders/triangle.frag.spv";
        cfg.color_attachment_count = 1;
        cfg.color_formats          = &renderer.hdr_color[1].format;

        cfg.depth_format = renderer.depth[1].format;
        cfg.polygon_mode = VK_POLYGON_MODE_FILL;

        render_pipelines.pipelines[TRIANGLE_PIPELINE] = create_graphics_pipeline(&renderer, &cfg);
        render_pipelines.pipelines[POSTPROCESS_PIPELINE] =
            create_compute_pipeline(&renderer, "compiledshaders/postprocess.comp.spv");

        printf("graphics hdr %d      ", cfg.color_formats[0]);
        printf(" color hdr %d      ", renderer.hdr_color[1].format);
        printf(" original color hdr %d", VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    BufferPool pool = {0};
    buffer_pool_init(&renderer, &pool, MB(256),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                     VMA_MEMORY_USAGE_AUTO,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 2048);

    VkBufferDeviceAddressInfo addrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = pool.buffer};
    TextureID        tex_id = load_texture(&renderer, "/home/lk/myprojects/flowgame/data/PNG/Tiles/greystone.png");
    static TextureID voxel_textures[TEX_COUNT];

    voxel_textures[TEX_STONE] = load_texture(&renderer, "/home/lk/myprojects/flowgame/data/PNG/Tiles/greystone.png");
    voxel_textures[TEX_GRASS_TOP] = load_texture(&renderer, "/home/lk/myprojects/flowgame/data/PNG/Tiles/grass_top.png");

    voxel_textures[TEX_GRASS_SIDE] = load_texture(&renderer, "/home/lk/myprojects/flowgame/data/PNG/Tiles/gravel_dirt.png");

    voxel_textures[TEX_DIRT] = load_texture(&renderer, "/home/lk/myprojects/flowgame/data/PNG/Tiles/dirt.png");


    SamplerCreateDesc desc = {.mag_filter = VK_FILTER_LINEAR,
                              .min_filter = VK_FILTER_LINEAR,

                              .address_u   = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .address_v   = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .address_w   = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                              .max_lod     = 1.0f};

    SamplerID linear_sampler = create_sampler(&renderer, &desc);


    BufferSlice indirect_slice = buffer_pool_alloc(&pool, sizeof(VkDrawIndirectCommand), 16);
    BufferSlice count_slice    = buffer_pool_alloc(&pool, sizeof(uint32_t), 4);

    VkDrawIndirectCommand* cpu_indirect = (VkDrawIndirectCommand*)indirect_slice.mapped;
    uint32_t*              cpu_count    = (uint32_t*)count_slice.mapped;
    /* indirect draw command */

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
    static Chunk chunk = {0};

    for(int x = 0; x < CHUNK_SIZE; x++)
        for(int z = 0; z < CHUNK_SIZE; z++)
        {
            float n = terrain(x, z);
            n       = n * 0.5f + 0.5f;

            int height = (int)(n * (CHUNK_SIZE - 1));
            height     = glm_clamp(height, 1, CHUNK_SIZE - 1);


            for(int y = 0; y < height; y++)
            {
                if(y == height - 1)
                    chunk.voxels[x][y][z].type = GRASS;
                else
                    chunk.voxels[x][y][z].type = STONE;
            }
        }


    ChunkMesh mesh;
    mesh.faces = malloc(sizeof(PackedFace) * 100000);

    build_chunk_mesh(&chunk, &mesh);

    for(uint32_t i = 0; i < mesh.face_count; i++)
    {
        uint32_t tex_slot = mesh.faces[i].data1;
        if(tex_slot < TEX_COUNT)
        {
            mesh.faces[i].data1 = voxel_textures[tex_slot];
        }
    }

    printf("voxel debug: face_count=%u\n", mesh.face_count);
    for(uint32_t i = 0; i < mesh.face_count && i < 4; i++)
    {
        uint32_t data0 = mesh.faces[i].data0;
        uint32_t x     = data0 & 255u;
        uint32_t y     = (data0 >> 8) & 255u;
        uint32_t z     = (data0 >> 16) & 4095u;
        uint32_t face  = (data0 >> 28) & 7u;
        printf("face[%u]: xyz=(%u,%u,%u) face=%u tex_id=%u\n", i, x, y, z, face, mesh.faces[i].data1);
    }

    BufferSlice face_slice = buffer_pool_alloc(&pool, sizeof(PackedFace) * mesh.face_count, 16);

    PackedFace* cpu_faces = (PackedFace*)face_slice.mapped;

    memcpy(cpu_faces, mesh.faces, sizeof(PackedFace) * mesh.face_count);

    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, face_slice.offset, sizeof(PackedFace) * mesh.face_count);

    VkDeviceAddress face_ptr    = vkGetBufferDeviceAddress(renderer.device, &addrInfo) + face_slice.offset;
    cpu_indirect[0].vertexCount = mesh.face_count * 6;
    ;
    cpu_indirect[0].instanceCount = 1;
    cpu_indirect[0].firstVertex   = 0;
    cpu_indirect[0].firstInstance = 0;

    *cpu_count = 1;

    /* flush */


    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, indirect_slice.offset, sizeof(VkDrawIndirectCommand));

    vmaFlushAllocation(renderer.vmaallocator, pool.allocation, count_slice.offset, sizeof(uint32_t));

    /* Push layout shared with shaders/triangle.slang
         face_ptr=0, face_count=8, aspect=12, pad0=16, pad1=20, pad2=24, pad3=28,
         view_proj=32, texture_id=96, sampler_id=100 */

    PUSH_CONSTANT(Push, VkDeviceAddress face_ptr; uint32_t face_count; float aspect; uint32_t pad0; uint32_t pad1;
                  uint32_t pad2; uint32_t pad3;

                  float view_proj[4][4];

                  uint32_t texture_id; uint32_t sampler_id;);
    PUSH_CONSTANT(PostPush, uint src_texture_id; uint output_image_id;

                  uint width; uint height;

                  float exposure; float gamma;

                  uint frame;)


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
            // image_transition_swapchain(cmd, &renderer.swapchain, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            //                            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
            rt_transition_all(cmd, &renderer.depth[renderer.swapchain.current_image], VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);
            rt_transition_all(cmd, &renderer.hdr_color[renderer.swapchain.current_image], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

            image_transition_swapchain(cmd, &renderer.swapchain, VK_IMAGE_LAYOUT_GENERAL,
                                       VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        }

        VkRenderingAttachmentInfo color = {.sType     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                           .imageView = renderer.hdr_color[renderer.swapchain.current_image].view,
                                           .imageLayout =
                                               renderer.hdr_color[renderer.swapchain.current_image].mip_states[0].layout,
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

            double cpu_frame_ms  = renderer.cpu_frame_ns / 1000000.0;
            double cpu_active_ms = renderer.cpu_active_ns / 1000000.0;
            double cpu_wait_ms   = renderer.cpu_wait_ns / 1000000.0;

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

                push.aspect = (float)renderer.swapchain.extent.width / (float)renderer.swapchain.extent.height;

                push.texture_id = tex_id;
                push.sampler_id = linear_sampler;


                push.face_ptr   = face_ptr;
                push.face_count = mesh.face_count;

                glm_mat4_copy(cam.view_proj, push.view_proj);

                vkCmdPushConstants(cmd, renderer.bindless_system.pipeline_layout, VK_SHADER_STAGE_ALL, 0, sizeof(Push), &push);

                vkCmdDraw(cmd, mesh.face_count * 6, 1, 0, 0);
            }

            {
                ImDrawData* draw_data = igGetDrawData();
                ImGui_ImplVulkan_RenderDrawData(draw_data, cmd, VK_NULL_HANDLE);
            }


            vkCmdEndRendering(cmd);
        }

        {


            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, render_pipelines.pipelines[POSTPROCESS_PIPELINE]);

            printf("HDR index = %u\n", renderer.hdr_color[renderer.swapchain.current_image].bindless_index);
            printf("LDR index = %u\n", renderer.ldr_color[renderer.swapchain.current_image].bindless_index);
            PostPush push       = {0};
            push.src_texture_id = renderer.hdr_color[renderer.swapchain.current_image].bindless_index;

            push.output_image_id = renderer.ldr_color[renderer.swapchain.current_image].bindless_index;

            push.width    = renderer.swapchain.extent.width;
            push.height   = renderer.swapchain.extent.height;
            push.exposure = 1.0f;
            push.gamma    = 2.2f;
            vkCmdPushConstants(cmd, renderer.bindless_system.pipeline_layout, VK_SHADER_STAGE_ALL, 0, sizeof(PostPush), &push);

            uint32_t gx = (push.width + 15) / 16;
            uint32_t gy = (push.height + 15) / 16;

            vkCmdDispatch(cmd, gx, gy, 1);
        }
        rt_transition_all(cmd, &renderer.ldr_color[renderer.swapchain.current_image], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        image_transition_swapchain(renderer.frames[renderer.current_frame].cmdbuf, &renderer.swapchain,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0);

        VkImageBlit blit = {
            .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
            .srcOffsets = {{0, 0, 0}, {renderer.swapchain.extent.width, renderer.swapchain.extent.height, 1}},

            .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
            .dstOffsets = {{0, 0, 0}, {renderer.swapchain.extent.width, renderer.swapchain.extent.height, 1}}};

        vkCmdBlitImage(cmd, renderer.ldr_color[renderer.swapchain.current_image].image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, renderer.swapchain.images[renderer.swapchain.current_image],
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_NEAREST);

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
