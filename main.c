#include "tinytypes.h"
#include "vk_default.h"
#include "vk.h"
#include <GLFW/glfw3.h>
#include <stdbool.h>
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
// renderer.pipelines[PIPELINE_TRIANGLE] =
//     create_graphics_pipeline(...);
//
// vkCmdBindPipeline(cmd,
//     VK_PIPELINE_BIND_POINT_GRAPHICS,
//     renderer.pipelines[PIPELINE_TRIANGLE]);
//
//
//
//

//i think we can only make one single indless gloal desc layout so we have to ind only that and we are free from managing desc per oject

// but because of different needs of push constants we might or might not maintain just one global pipeline layout
//   Bindful is object-oriented thinking. “Each draw has its state.”
//Bindless is data-oriented thinking. “All resources live in big tables.”
//
//     
// set 0 binding 0: texture2D images[100000]
// set 0 binding 1: sampler samplers[32]
// set 0 binding 2: buffer storageBuffers[100000]
// set 0 binding 3: buffer uniformBuffers[10000]
// set 0 binding 4: image2D storageImages[10000]
// set 0 binding 5: buffer vertexBuffers[100000]
// set 0 binding 6: buffer indexBuffers[100000]
// set 0 binding 7: buffer materials[100000]
// set 0 binding 8: buffer transforms[100000]
//





#define VALIDATION true
static bool g_framebuffer_resized = false;
static void framebuffer_resize_callback(GLFWwindow* window, int width, int height)
{
    (void)window;
    (void)width;
    (void)height;
    g_framebuffer_resized = true;
}
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
    };
    Renderer renderer = {0};
    renderer_create(&renderer, &desc);


    GraphicsPipelineConfig cfg = pipeline_config_default();
    cfg.vert_path              = "compiledshaders/triangle.vert.spv";
    cfg.frag_path              = "compiledshaders/triangle.frag.spv";
    cfg.color_attachment_count = 1;
    cfg.color_formats          = &renderer.swapchain.format;
    VkPipeline trianglepipe    = create_graphics_pipeline(&renderer, &cfg);

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

        vkWaitForFences(renderer.device, 1, &renderer.frames[renderer.current_frame].in_flight_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(renderer.device, 1, &renderer.frames[renderer.current_frame].in_flight_fence);

        /* reset EVERYTHING allocated for this frame */
        vkResetCommandPool(renderer.device, renderer.frames[renderer.current_frame].cmdbufpool, 0);
        vk_swapchain_acquire(renderer.device, &renderer.swapchain,
                             renderer.frames[renderer.current_frame].image_available_semaphore, VK_NULL_HANDLE, UINT64_MAX);

        vk_cmd_begin(renderer.frames[renderer.current_frame].cmdbuf, true);
        image_transition_swapchain(renderer.frames[renderer.current_frame].cmdbuf, &renderer.swapchain,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

	VkRenderingAttachmentInfo color = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                           .imageView = renderer.swapchain.image_views[renderer.swapchain.current_image],
                                           .imageLayout = renderer.swapchain.states[renderer.swapchain.current_image].layout,
                                           .loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                           .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                           .clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}}};

	VkRenderingInfo rendering = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                     .renderArea.extent = renderer.swapchain.extent,
                                     .layerCount = 1,
                                     .colorAttachmentCount = 1,
                                     .pColorAttachments    = &color};

	vkCmdBeginRendering(renderer.frames[renderer.current_frame].cmdbuf, &rendering);
        vkCmdBindPipeline(renderer.frames[renderer.current_frame].cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglepipe);
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
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
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
    return 0;
}
