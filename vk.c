#include "vk.h"
#include "tinytypes.h"
#include <stdbool.h>
#include <vulkan/vulkan_core.h>


#define DEBUG

bool is_instance_extension_supported(const char* extension_name)
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    VkExtensionProperties* extensions = malloc(extensionCount * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions);

    for(uint32_t i = 0; i < extensionCount; i++)
    {
        if(strcmp(extension_name, extensions[i].extensionName) == 0)
        {
            free(extensions);
            return true;
        }
    }

    free(extensions);
    return false;
}
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT             type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                     void*                                       user_data)
{
    (void)type;
    (void)user_data;


    const char* tag = "MSG";

    if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        tag = "ERROR";
    else if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        tag = "WARN";
    else if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        tag = "INFO";
    else if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        tag = "VERBOSE";

    fprintf(stderr, "[VULKAN %s] %s\n", tag, data->pMessage);

    return VK_FALSE;
}


bool device_supports_extensions(VkPhysicalDevice gpu, const char** req, uint32_t req_count)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(gpu, NULL, &count, NULL);

    VkExtensionProperties* props = malloc(sizeof(*props) * count);
    vkEnumerateDeviceExtensionProperties(gpu, NULL, &count, props);

    for(uint32_t i = 0; i < req_count; i++)
    {
        bool found = false;

        for(uint32_t j = 0; j < count; j++)
        {
            if(strcmp(req[i], props[j].extensionName) == 0)
            {
                found = true;
                break;
            }
        }

        if(!found)
        {
            log_error("[extensions] missing: %s", req[i]);
            free(props);
            return false;
        }

        log_info("[extensions] enabled: %s", req[i]);
    }

    free(props);
    return true;
}


typedef struct GpuScore
{
    VkPhysicalDevice device;
    uint32_t         score;
} GpuScore;

static uint32_t score_physical_device(VkPhysicalDevice gpu, VkSurfaceKHR surface, const char** required_exts, uint32_t required_ext_count)
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(gpu, &props);

    fprintf(stderr, "[GPU] Evaluating: %s\n", props.deviceName);

    if(!device_supports_extensions(gpu, required_exts, required_ext_count))
    {
        fprintf(stderr, "  -> rejected: missing required extensions\n");
        return 0;
    }

    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count, NULL);

    VkQueueFamilyProperties qprops[32];
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count, qprops);

    VkBool32 can_present = VK_FALSE;
    for(uint32_t i = 0; i < queue_count; i++)
    {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &present);
        if(present)
        {
            can_present = VK_TRUE;
            break;
        }
    }

    if(!can_present)
    {
        fprintf(stderr, "  -> rejected: cannot present to surface\n");
        return 0;
    }

    uint32_t score = 0;

    switch(props.deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 1000;
            fprintf(stderr, "  + discrete bonus: 1000\n");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 600;
            fprintf(stderr, "  + integrated bonus: 600\n");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 300;
            fprintf(stderr, "  + virtual bonus: 300\n");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 50;
            fprintf(stderr, "  + cpu fallback: 50\n");
            break;
        default:
            fprintf(stderr, "  + unknown type: 0\n");
            break;
    }

    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(gpu, &mem);

    for(uint32_t i = 0; i < mem.memoryHeapCount; i++)
    {
        if(mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            uint32_t add = (uint32_t)(mem.memoryHeaps[i].size / (1024 * 1024 * 64));
            score += add;
            fprintf(stderr, "  + VRAM factor: %u\n", add);
        }
    }

    if(score == 0)
        score = 1;

    fprintf(stderr, "  -> final score: %u\n\n", score);
    return score;
}

VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface, RendererDesc* rcd)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    if(count == 0)
    {
        fprintf(stderr, "[GPU] No Vulkan devices found. Tragic.\n");
        return VK_NULL_HANDLE;
    }

    VkPhysicalDevice devices[16];
    vkEnumeratePhysicalDevices(instance, &count, devices);

    GpuScore best = {0};

    log_info("[GPU] Found %u device(s). Scoring...", count);

    for(uint32_t i = 0; i < count; i++)
    {
        uint32_t score = score_physical_device(devices[i], surface, rcd->device_extensions, rcd->device_extension_count);

        if(score > best.score)
        {
            best.device = devices[i];
            best.score  = score;
        }
    }

    if(best.device == VK_NULL_HANDLE)
    {
        fprintf(stderr, "[GPU] No suitable device found. Time to rethink life choices.\n");
    }
    else
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(best.device, &props);
        log_info("[GPU] Selected device: %s (score %u)", props.deviceName, best.score);
    }

    return best.device;
}

void query_device_features(VkPhysicalDevice gpu, VkFeatureChain* out)
{
    memset(out, 0, sizeof(*out));

    out->core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    out->v11.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    out->v12.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    out->v13.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    // maintenance5 feature struct
    out->maintenance5.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR;

    // Chain: core -> v11 -> v12 -> v13 -> maintenance5
    out->core.pNext = &out->v11;
    out->v11.pNext  = &out->v12;
    out->v12.pNext  = &out->v13;
    out->v13.pNext  = &out->maintenance5;

    vkGetPhysicalDeviceFeatures2(gpu, &out->core);
}


typedef struct RendererCaps
{
    bool dynamic_rendering;
    bool sync2;
    bool descriptor_indexing;
    bool timeline_semaphores;
    bool multi_draw_indirect;
    bool multi_draw_indirect_count;
    bool buffer_device_address;
    bool maintenance4;
    bool bindless_textures;

    bool sampler_anisotropy;     // NEW
    bool atomic_int64;           // NEW
    bool scalar_block_layout;    // NEW
    bool robustness2;            // NEW
    bool index_type_uint8;       // NEW
    bool subgroup_size_control;  // NEW
} RendererCaps;

RendererCaps default_caps(void)
{
    return (RendererCaps){
        .dynamic_rendering         = true,
        .sync2                     = true,
        .descriptor_indexing       = true,
        .timeline_semaphores       = true,
        .multi_draw_indirect       = true,
        .multi_draw_indirect_count = true,
        .buffer_device_address     = true,
        .maintenance4              = true,
        .bindless_textures         = true,

        .sampler_anisotropy    = true,
        .atomic_int64          = true,
        .scalar_block_layout   = true,
        .robustness2           = false,  // I’ll explain below
        .index_type_uint8      = true,
        .subgroup_size_control = false,  // enable later if you need it
    };
}
static void enable_desc_indexing_feature(VkBool32* feature_field, const char* name)
{
    // feature_field currently contains "supported?" from vkGetPhysicalDeviceFeatures2
    if(*feature_field)
    {
        *feature_field = VK_TRUE;  // enable
        log_info("[bindless] enabled: %s", name);
    }
    else
    {
        log_info("[bindless] NOT available: %s", name);
        // leave it VK_FALSE
    }
}
static void apply_caps(VkFeatureChain* f, const RendererCaps* caps)
{
#define TRY_ENABLE(flag, supported, name)                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if((caps->flag) && (supported))                                                                                \
        {                                                                                                              \
            (supported) = VK_TRUE;                                                                                     \
            log_info("[features] enabled: %s", name);                                                                  \
        }                                                                                                              \
        else if(caps->flag)                                                                                            \
        {                                                                                                              \
            log_info("[features] unavailable: %s", name);                                                              \
        }                                                                                                              \
    } while(0)
    if(f->v11.shaderDrawParameters)
    {
        f->v11.shaderDrawParameters = VK_TRUE;
        log_info("[features] enabled: shaderDrawParameters (vulkan 1.1)");
    }
    else
    {
        log_info("[features] unavailable: shaderDrawParameters (vulkan 1.1)");
    }
    if(f->maintenance5.maintenance5)
    {
        f->maintenance5.maintenance5 = VK_TRUE;
        log_info("[features] enabled: maintenance5 (VK_KHR_maintenance5)");
    }
    else
    {
        log_info("[features] unavailable: maintenance5 (VK_KHR_maintenance5)");
    }
    TRY_ENABLE(sampler_anisotropy, f->core.features.samplerAnisotropy, "samplerAnisotropy");
    TRY_ENABLE(dynamic_rendering, f->v13.dynamicRendering, "dynamic rendering");
    TRY_ENABLE(sync2, f->v13.synchronization2, "synchronization2");
    TRY_ENABLE(descriptor_indexing, f->v12.descriptorIndexing, "descriptor indexing (vulkan 1.2)");
    TRY_ENABLE(timeline_semaphores, f->v12.timelineSemaphore, "timeline semaphores");
    TRY_ENABLE(multi_draw_indirect, f->core.features.multiDrawIndirect, "multi-draw indirect");
    TRY_ENABLE(multi_draw_indirect_count, f->v12.drawIndirectCount, "multi-draw indirect count (v1.2)");
    TRY_ENABLE(buffer_device_address, f->v12.bufferDeviceAddress, "buffer device address");
    TRY_ENABLE(maintenance4, f->v13.maintenance4, "maintenance4");

    if(caps->bindless_textures)
    {
        if(!f->v12.descriptorIndexing)
        {
            log_info("[bindless] descriptor indexing umbrella feature not supported -> bindless likely impossible");
        }


        enable_desc_indexing_feature(&f->v12.runtimeDescriptorArray, "runtimeDescriptorArray");

        enable_desc_indexing_feature(&f->v12.descriptorBindingVariableDescriptorCount, "descriptorBindingVariableDescriptorCount");

        enable_desc_indexing_feature(&f->v12.descriptorBindingPartiallyBound, "descriptorBindingPartiallyBound");

        enable_desc_indexing_feature(&f->v12.descriptorBindingSampledImageUpdateAfterBind, "descriptorBindingSampledImageUpdateAfterBind");

        enable_desc_indexing_feature(&f->v12.shaderSampledImageArrayNonUniformIndexing, "shaderSampledImageArrayNonUniformIndexing");
    }

#undef TRY_ENABLE
}

typedef struct queue_families
{
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;

    uint32_t graphics_family;
    uint32_t present_family;
    uint32_t compute_family;
    uint32_t transfer_family;

    int has_graphics;
    int has_present;
    int has_compute;
    int has_transfer;
} queue_families;
//  pick GPU → choose queue families → create VkDevice → get queues
// Fills `out` with available queue families.
// Must be called BEFORE logical device creation.
void find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface, queue_families* out);
// Call AFTER vkCreateDevice.
// Uses the family indices already stored in queue_families.
void init_device_queues(VkDevice device, queue_families* q);


void find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface, queue_families* out)
{
    // initialize everything
    *out = (queue_families){.graphics_queue = VK_NULL_HANDLE,
                            .present_queue  = VK_NULL_HANDLE,
                            .compute_queue  = VK_NULL_HANDLE,
                            .transfer_queue = VK_NULL_HANDLE,

                            .graphics_family = 0,
                            .present_family  = 0,
                            .compute_family  = 0,
                            .transfer_family = 0,

                            .has_graphics = 0,
                            .has_present  = 0,
                            .has_compute  = 0,
                            .has_transfer = 0};

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
    if(count == 0)
        return;

    VkQueueFamilyProperties* families = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);

    if(!families)
        return;

    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families);

    for(uint32_t i = 0; i < count; i++)
    {
        const VkQueueFamilyProperties* f = &families[i];

        if(!out->has_graphics && (f->queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            out->graphics_family = i;
            out->has_graphics    = 1;
        }

        if(!out->has_compute && (f->queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            out->compute_family = i;
            out->has_compute    = 1;
        }

        if(!out->has_transfer && (f->queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            out->transfer_family = i;
            out->has_transfer    = 1;
        }

        if(!out->has_present)
        {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if(presentSupport)
            {
                out->present_family = i;
                out->has_present    = 1;
            }
        }

        if(out->has_graphics && out->has_present && out->has_compute && out->has_transfer)
        {
            break;
        }
    }

    free(families);
}

void init_device_queues(VkDevice device, queue_families* q)
{
    if(q->has_graphics)
        vkGetDeviceQueue(device, q->graphics_family, 0, &q->graphics_queue);

    if(q->has_present)
        vkGetDeviceQueue(device, q->present_family, 0, &q->present_queue);

    if(q->has_compute)
        vkGetDeviceQueue(device, q->compute_family, 0, &q->compute_queue);

    if(q->has_transfer)
        vkGetDeviceQueue(device, q->transfer_family, 0, &q->transfer_queue);
}
static bool device_has_extension(VkPhysicalDevice gpu, const char* ext)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(gpu, NULL, &count, NULL);

    VkExtensionProperties* props = malloc(sizeof(*props) * count);
    vkEnumerateDeviceExtensionProperties(gpu, NULL, &count, props);

    bool found = false;
    for(uint32_t i = 0; i < count; i++)
    {
        if(strcmp(props[i].extensionName, ext) == 0)
        {
            found = true;
            break;
        }
    }

    free(props);
    return found;
}

void renderer_create(Renderer* r, RendererDesc* desc)
{

    // Instance
    // Debug messenger
    // Physical device
    // Device info
    // Logical device
    // Queues
    // Frame contexts

    memset(r, 0, sizeof(*r));
    {


        VkApplicationInfo app = {.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                 .pApplicationName   = desc->app_name,
                                 .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                 .pEngineName        = "FLOW",
                                 .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
                                 .apiVersion         = VK_API_VERSION_1_3

        };

        const char* extensions[64];
        uint32_t    ext_count = 0;

        forEach(i, desc->instance_extension_count)
        {
            extensions[ext_count++] = desc->instance_extensions[i];
        }


        extensions[ext_count++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;

        if(desc->enable_validation)
        {
            extensions[ext_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }

        const char* layers[8];
        uint32_t    layer_count = desc->instance_layer_count;

        if(layer_count)
        {
            memcpy(layers, desc->instance_layers, sizeof(char*) * layer_count);
        }
        if(desc->enable_validation)
        {
            layers[layer_count++] = "VK_LAYER_KHRONOS_validation";
        }

        VkValidationFeaturesEXT validation_features;
        memset(&validation_features, 0, sizeof(validation_features));
        validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        //todo get this from rendererdesc may be
        static const VkValidationFeatureEnableEXT enabled_features[] = {
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
            VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT, VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
            VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};

        log_info("[instance] app=%s api=1.3 validation=%u gpu_validation=%u",
                 desc->app_name ? desc->app_name : "(null)", desc->enable_validation, desc->enable_gpu_based_validation);
        forEach(i, ext_count) log_info("[instance]  ext[%u]=%s", i, extensions[i]);

        if(layer_count > 0)
        {
            log_info("[instance] layers: %u", layer_count);
            for(uint32_t i = 0; i < layer_count; i++)
                log_info("[instance]  layer[%u]=%s", i, layers[i]);
        }
        VkInstanceCreateInfo ci = {.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                   .pApplicationInfo        = &app,
                                   .enabledExtensionCount   = ext_count,
                                   .ppEnabledExtensionNames = extensions,
                                   .enabledLayerCount       = layer_count,
                                   .ppEnabledLayerNames     = layers};
        if(desc->enable_validation && desc->enable_gpu_based_validation)
        {
            validation_features.enabledValidationFeatureCount =
                (uint32_t)(sizeof(enabled_features) / sizeof(enabled_features[0]));
            validation_features.pEnabledValidationFeatures = enabled_features;

            validation_features.pNext = ci.pNext;
            ci.pNext                  = &validation_features;
        }
        VK_CHECK(vkCreateInstance(&ci, r->instance.allocator, &r->instance.instance));
        volkLoadInstance(r->instance.instance);
        log_info("[renderer] instance created");
    }


    {


#ifdef DEBUG
        //
        // 2. Debug Messenger
        //
        if(desc->enable_validation)
        {
            VkDebugUtilsMessengerCreateInfoEXT ci = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,

                                                     .messageSeverity = desc->validation_severity,

                                                     .messageType = desc->validation_types,

                                                     .pfnUserCallback = debug_callback};

            PFN_vkCreateDebugUtilsMessengerEXT fn =
                (void*)vkGetInstanceProcAddr(r->instance.instance, "vkCreateDebugUtilsMessengerEXT");

            if(fn)
            {
                fn(r->instance.instance, &ci, r->instance.allocator, &r->instance.debug_messenger);

                log_info("[renderer] debug messenger created");
            }
        }
#endif
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    r->window = glfwCreateWindow(desc->width, desc->height, "Vulkan", NULL, NULL);

    VK_CHECK(glfwCreateWindowSurface(r->instance.instance, r->window, NULL, &r->surface));
    //
    // 3. Pick Physical Device
    //
    {
        r->physical_device = pick_physical_device(r->instance.instance, r->surface, desc);


        if(r->physical_device == VK_NULL_HANDLE)
        {
            log_error("No GPU found");
            return;
        }

        vkGetPhysicalDeviceProperties(r->physical_device, &r->info.properties);

        vkGetPhysicalDeviceMemoryProperties(r->physical_device, &r->info.memory);

        log_info("[renderer] GPU: %s", r->info.properties.deviceName);
    }


    // 4. Query Features
    //
    if(desc->use_custom_features)
    {
        r->info.feature_chain = desc->custom_features;
    }
    else
    {
        query_device_features(r->physical_device, &r->info.feature_chain);

        RendererCaps caps = default_caps();

        apply_caps(&r->info.feature_chain, &caps);
    }

    //
    // 5. Find Queue Families
    //
    queue_families q;
    find_queue_families(r->physical_device, r->surface, &q);

    r->graphics_queue_index = q.graphics_family;

    r->present_queue_index = q.present_family;

    r->compute_queue_index = q.has_compute ? q.compute_family : q.graphics_family;

    r->transfer_queue_index = q.has_transfer ? q.transfer_family : q.graphics_family;


    //
    // 6. Create Logical Device
    //
    {
        float priority = 1.0f;

        uint32_t unique[4];
        uint32_t count = 0;

        unique[count++] = r->graphics_queue_index;

        if(r->present_queue_index != r->graphics_queue_index)
            unique[count++] = r->present_queue_index;

        if(r->compute_queue_index != r->graphics_queue_index)
            unique[count++] = r->compute_queue_index;

        if(r->transfer_queue_index != r->graphics_queue_index)
            unique[count++] = r->transfer_queue_index;

        VkDeviceQueueCreateInfo queues[4];

        for(uint32_t i = 0; i < count; i++)
        {
            queues[i] = (VkDeviceQueueCreateInfo){.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,

                                                  .queueFamilyIndex = unique[i],

                                                  .queueCount = 1,

                                                  .pQueuePriorities = &priority};
        }

        const char* extensions[32];
        uint32_t    ext_count = 0;

        for(uint32_t i = 0; i < desc->device_extension_count; i++)
        {
            extensions[ext_count++] = desc->device_extensions[i];
        }

        if(device_has_extension(r->physical_device, VK_KHR_MAINTENANCE_5_EXTENSION_NAME))
        {
            extensions[ext_count++] = VK_KHR_MAINTENANCE_5_EXTENSION_NAME;
        }

        VkDeviceCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,

                                 .pNext = &r->info.feature_chain.core,

                                 .queueCreateInfoCount = count,

                                 .pQueueCreateInfos = queues,

                                 .enabledExtensionCount = ext_count,

                                 .ppEnabledExtensionNames = extensions};

        VK_CHECK(vkCreateDevice(r->physical_device, &ci, r->allocator, &r->device));

        volkLoadDevice(r->device);
        log_info("[renderer] logical device created");
    }


    //
    // 7. Get Queues
    //
    vkGetDeviceQueue(r->device, r->graphics_queue_index, 0, &r->graphics_queue);
    vkGetDeviceQueue(r->device, r->present_queue_index, 0, &r->present_queue);

    vkGetDeviceQueue(r->device, r->compute_queue_index, 0, &r->compute_queue);

    vkGetDeviceQueue(r->device, r->transfer_queue_index, 0, &r->transfer_queue);

    log_info("[renderer] queues acquired");

    //
    // 8. Create Frame Contexts
    //
    for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        FrameContext* f = &r->frames[i];
        vk_cmd_create_pool(r->device, r->graphics_queue_index, true, false, &f->cmdbufpool);
        vk_cmd_alloc(r->device, f->cmdbufpool, true, &f->cmdbuf);
        vk_create_semaphore(r->device, &f->image_available_semaphore);
        vk_create_fence(r->device, true, &f->in_flight_fence);
    }

    log_info("[renderer] frame contexts created");

    r->current_frame = 0;

    log_info("[renderer] initialization complete");


    vk_cmd_create_pool(r->device, r->graphics_queue_index, true, false, &r->one_time_gfx_pool);
}


VkSurfaceCapabilities2KHR query_surface_capabilities(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    VkPhysicalDeviceSurfaceInfo2KHR info = {
        .sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .surface = surface,
    };

    VkSurfaceCapabilities2KHR caps = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
    };

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(gpu, &info, &caps));
    return caps;
}


VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR* caps, uint32_t desired_w, uint32_t desired_h)
{
    if(caps->currentExtent.width != 0xFFFFFFFF)
        return caps->currentExtent;
    VkExtent2D extent = {.width = desired_w, .height = desired_h};
    extent.width      = CLAMP(extent.width, caps->minImageExtent.width, caps->maxImageExtent.width);
    extent.height     = CLAMP(extent.height, caps->minImageExtent.height, caps->maxImageExtent.height);
    return extent;
}


VkSurfaceFormatKHR select_surface_format(VkPhysicalDevice gpu, VkSurfaceKHR surface, VkFormat preferred, VkColorSpaceKHR preferred_cs)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, NULL);

    VkSurfaceFormatKHR formats[32];
    if(count > 32)
        count = 32;

    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, formats);

    for(uint32_t i = 0; i < count; ++i)
        if(formats[i].format == preferred && formats[i].colorSpace == preferred_cs)
            return formats[i];

    return formats[0];
}

// Choose minImageCount given a user hint, but always respect Vulkan caps.
static uint32_t choose_min_image_count(const VkSurfaceCapabilities2KHR* caps, uint32_t preferred_hint)
{
    const uint32_t min_cap = caps->surfaceCapabilities.minImageCount;

    // Never go below Vulkan's minimum, even if the hint is silly.
    uint32_t preferred = (preferred_hint > min_cap) ? preferred_hint : min_cap;

    // maxImageCount == 0 means "no upper bound"
    const uint32_t raw_max = caps->surfaceCapabilities.maxImageCount;
    const uint32_t max_cap = (raw_max == 0) ? preferred : raw_max;

    // Clamp to [min_cap, max_cap]
    if(preferred < min_cap)
        preferred = min_cap;
    if(preferred > max_cap)
        preferred = max_cap;

    return preferred;
}
void vk_create_swapchain(VkDevice                       device,
                         VkPhysicalDevice               gpu,
                         FlowSwapchain*                 out_swapchain,
                         const FlowSwapchainCreateInfo* info,
                         VkQueue                        graphics_queue,
                         VkCommandPool                  one_time_pool)
{
    VkSurfaceCapabilities2KHR caps = query_surface_capabilities(gpu, info->surface);

    // Query formats and present modes up-front to satisfy validation and pick supported values.
    VkSurfaceFormatKHR surface_format =
        select_surface_format(gpu, info->surface, info->preferred_format, info->preferred_color_space);
    VkPresentModeKHR present_mode = vk_swapchain_select_present_mode(gpu, info->surface, false);
    if(info->preferred_present_mode != VK_PRESENT_MODE_MAX_ENUM_KHR)
        present_mode = info->preferred_present_mode;

    VkExtent2D extent = choose_extent(&caps.surfaceCapabilities, info->width, info->height);

    if(extent.width == 0 || extent.height == 0)
        return;  // minimized, wait later


    VkImageUsageFlags usage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | info->extra_usage) & caps.surfaceCapabilities.supportedUsageFlags;
    VkSwapchainCreateInfoKHR ci = {.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                   .surface          = info->surface,
                                   .minImageCount    = choose_min_image_count(&caps, info->min_image_count),
                                   .imageFormat      = surface_format.format,
                                   .imageColorSpace  = surface_format.colorSpace,
                                   .imageExtent      = extent,
                                   .imageArrayLayers = 1,
                                   .imageUsage       = usage,
                                   .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                   .preTransform     = caps.surfaceCapabilities.currentTransform,
                                   .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                   .presentMode      = present_mode,
                                   .clipped          = VK_TRUE,
                                   .oldSwapchain     = info->old_swapchain};

    log_info("[swapchain] create: extent=%ux%u minImageCount=%u format=%u colorSpace=%u presentMode=%u usage=0x%x",
             extent.width, extent.height, ci.minImageCount, ci.imageFormat, ci.imageColorSpace, ci.presentMode, usage);

    VK_CHECK(vkCreateSwapchainKHR(device, &ci, NULL, &out_swapchain->swapchain));

    out_swapchain->surface       = info->surface;
    out_swapchain->extent        = extent;
    out_swapchain->format        = surface_format.format;
    out_swapchain->color_space   = surface_format.colorSpace;
    out_swapchain->present_mode  = present_mode;
    out_swapchain->current_image = 0;
    out_swapchain->image_usage   = usage;
    // Query swapchain images
    VK_CHECK(vkGetSwapchainImagesKHR(device, out_swapchain->swapchain, &out_swapchain->image_count, NULL));
    log_info("[swapchain] images: %u", out_swapchain->image_count);

    if(out_swapchain->image_count > MAX_SWAPCHAIN_IMAGES)
        out_swapchain->image_count = MAX_SWAPCHAIN_IMAGES;  // don’t blow the stack

    VK_CHECK(vkGetSwapchainImagesKHR(device, out_swapchain->swapchain, &out_swapchain->image_count, out_swapchain->images));

    // Create image views
    forEach(i, out_swapchain->image_count)
    {
        VkImageViewCreateInfo view_ci = VK_IMAGE_VIEW_DEFAULT(out_swapchain->images[i], out_swapchain->format);
        VK_CHECK(vkCreateImageView(device, &view_ci, NULL, &out_swapchain->image_views[i]));
    }

    // Optional: transition all swapchain images UNDEFINED -> PRESENT
    {
        VkCommandBuffer cmd = begin_one_time_cmd(device, one_time_pool);

        forEach(i, out_swapchain->image_count)
        {
            VkImageMemoryBarrier2 barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask  = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = 0,

                .dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                .dstAccessMask = 0,

                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,

                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                .image = out_swapchain->images[i],

                .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .subresourceRange.baseMipLevel   = 0,
                .subresourceRange.levelCount     = 1,
                .subresourceRange.baseArrayLayer = 0,
                .subresourceRange.layerCount     = 1,
            };

            VkDependencyInfo dep = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,

                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers    = &barrier,
            };

            vkCmdPipelineBarrier2(cmd, &dep);
        }

        end_one_time_cmd(device, graphics_queue, one_time_pool, cmd);
    }

    vk_create_semaphores(device, out_swapchain->image_count, out_swapchain->render_finished);
}


void vk_swapchain_destroy(VkDevice device, FlowSwapchain* swapchain)
{
    if(!swapchain)
        return;

    forEach(i, swapchain->image_count)
    {
        if(swapchain->image_views[i] != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, swapchain->image_views[i], NULL);
        }
    }

    vk_destroy_semaphores(device, swapchain->image_count, swapchain->render_finished);
    if(swapchain->swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapchain->swapchain, NULL);
    }

    memset(swapchain, 0, sizeof(*swapchain));
}


bool vk_swapchain_acquire(VkDevice device, FlowSwapchain* sc, VkSemaphore image_available, VkFence fence, uint64_t timeout)
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

bool vk_swapchain_present(VkQueue present_queue, FlowSwapchain* sc, const VkSemaphore* waits, uint32_t wait_count)
{
    VkPresentInfoKHR info = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = wait_count,
        .pWaitSemaphores    = waits,
        .swapchainCount     = 1,
        .pSwapchains        = &sc->swapchain,
        .pImageIndices      = &sc->current_image,
    };

    VkResult r = vkQueuePresentKHR(present_queue, &info);

    if(r == VK_SUBOPTIMAL_KHR || r == VK_ERROR_OUT_OF_DATE_KHR)
    {
        sc->needs_recreate = true;
        return false;
    }

    VK_CHECK(r);
    return true;
}
VkPresentModeKHR vk_swapchain_select_present_mode(VkPhysicalDevice physical_device, VkSurfaceKHR surface, bool vsync)
{
    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, NULL);

    VkPresentModeKHR modes[16];
    if(mode_count > 16)
        mode_count = 16;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, modes);

    if(vsync)
    {
        /* Prefer FIFO (always available) */
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /* Prefer mailbox for low-latency without tearing */
    for(uint32_t i = 0; i < mode_count; i++)
    {
        if(modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    /* Fall back to immediate */
    for(uint32_t i = 0; i < mode_count; i++)
    {
        if(modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}


void vk_swapchain_recreate(VkDevice device, VkPhysicalDevice gpu, FlowSwapchain* sc, uint32_t new_w, uint32_t new_h, VkQueue graphics_queue, VkCommandPool one_time_pool)


{
    if(new_w == 0 || new_h == 0)
        return;
    vkDeviceWaitIdle(device);


    forEach(i, sc->image_count)
    {
        if(sc->image_views[i])
            vkDestroyImageView(device, sc->image_views[i], NULL);
    }

    vk_destroy_semaphores(device, sc->image_count, sc->render_finished);
    FlowSwapchainCreateInfo info = {0};
    info.surface                 = sc->surface;
    info.width                   = new_w;
    info.height                  = new_h;
    info.min_image_count         = MAX(3u, sc->image_count);
    info.preferred_format        = sc->format;
    info.preferred_color_space   = sc->color_space;
    info.preferred_present_mode  = sc->present_mode;
    info.extra_usage             = sc->image_usage & ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.old_swapchain           = sc->swapchain;

    VkSwapchainKHR old = sc->swapchain;

    vk_create_swapchain(device, gpu, sc, &info, graphics_queue, one_time_pool);

    if(old)
        vkDestroySwapchainKHR(device, old, NULL);
}
