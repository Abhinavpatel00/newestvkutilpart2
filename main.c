#include "vk_default.h"
#include "vk.h"
#include <GLFW/glfw3.h>


// Renderer
//     ├── InstanceContext
//     ├── PhysicalDeviceInfo
//     ├── DeviceContext
//     ├── SwapchainManager
//     ├── DescriptorAllocator (global)
//     ├── DescriptorAllocator (per-frame)
//     ├── PipelineCacheManager

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

    int fb_w, fb_h;
    glfwGetFramebufferSize(renderer.window, &fb_w, &fb_h);
    FlowSwapchainCreateInfo sci = {.surface         = renderer.surface,
                                   .width           = fb_w,
                                   .height          = fb_h,
                                   .min_image_count = 3,
                                   .preferred_present_mode =
                                       vk_swapchain_select_present_mode(renderer.physical_device, renderer.surface, false),
                                   //.preferred_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR,
                                   .preferred_format      = VK_FORMAT_B8G8R8A8_UNORM,
                                   .preferred_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                                   .extra_usage   = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                   .old_swapchain = VK_NULL_HANDLE};


    vk_create_swapchain(renderer.device, renderer.physical_device, &renderer.swapchain, &sci, renderer.graphics_queue,
                        renderer.one_time_gfx_pool);

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


        vk_swapchain_acquire(renderer.device, &renderer.swapchain,
                             renderer.frames[renderer.current_frame].image_available_semaphore, VK_NULL_HANDLE, UINT64_MAX);
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

            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos    = &wait_info,

            .commandBufferInfoCount = 0,
            .pCommandBufferInfos    = NULL,

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
