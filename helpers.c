#include "helpers.h"
void vk_create_fence(VkDevice device, bool signaled, VkFence* out_fence)
{
    VkFenceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u,
    };

    VK_CHECK(vkCreateFence(device, &info, NULL, out_fence));
}

void vk_create_fences(VkDevice device, uint32_t count, bool signaled, VkFence* out_fences)
{
    for (uint32_t i = 0; i < count; i++)
        vk_create_fence(device, signaled, &out_fences[i]);
}

void vk_wait_fence(VkDevice device, VkFence fence, uint64_t timeout_ns)
{
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, timeout_ns));
}

void vk_wait_fences(VkDevice device, uint32_t count,
                    const VkFence* fences, bool wait_all, uint64_t timeout_ns)
{
    VK_CHECK(vkWaitForFences(device, count, fences,
                             wait_all ? VK_TRUE : VK_FALSE, timeout_ns));
}

void vk_reset_fence(VkDevice device, VkFence fence)
{
    VK_CHECK(vkResetFences(device, 1, &fence));
}

void vk_reset_fences(VkDevice device, uint32_t count, const VkFence* fences)
{
    VK_CHECK(vkResetFences(device, count, fences));
}

bool vk_fence_is_signaled(VkDevice device, VkFence fence)
{
    return vkGetFenceStatus(device, fence) == VK_SUCCESS;
}

void vk_destroy_fences(VkDevice device, uint32_t count, VkFence* fences)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (fences[i] != VK_NULL_HANDLE)
        {
            vkDestroyFence(device, fences[i], NULL);
            fences[i] = VK_NULL_HANDLE;
        }
    }
}

/* ============================================================================
 * Semaphore Helpers
 * ============================================================================ */

void vk_create_semaphore(VkDevice device, VkSemaphore* out_semaphore)
{
    VkSemaphoreCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VK_CHECK(vkCreateSemaphore(device, &info, NULL, out_semaphore));
}

void vk_create_semaphores(VkDevice device, uint32_t count, VkSemaphore* out_semaphores)
{
    for (uint32_t i = 0; i < count; i++)
        vk_create_semaphore(device, &out_semaphores[i]);
}

void vk_destroy_semaphores(VkDevice device, uint32_t count, VkSemaphore* semaphores)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (semaphores[i] != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, semaphores[i], NULL);
            semaphores[i] = VK_NULL_HANDLE;
        }
    }
}


static VkCommandBufferLevel vk_cmd_level(bool primary)
{
    return primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
}

void vk_cmd_create_pool(VkDevice device, uint32_t queue_family_index, bool transient, bool resettable, VkCommandPool* out_pool)
{
    uint32_t flags = 0;

    if(transient)
        flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if(resettable)
        flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPoolCreateInfo ci = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = NULL,
        .flags            = flags,
        .queueFamilyIndex = queue_family_index,
    };

    VK_CHECK(vkCreateCommandPool(device, &ci, NULL, out_pool));
    log_info("[cmd] command pool created qf=%u flags=0x%x", queue_family_index, flags);
}

void vk_cmd_destroy_pool(VkDevice device, VkCommandPool pool)
{
    if(pool)
        vkDestroyCommandPool(device, pool, NULL);
}

void vk_cmd_create_many_pools(VkDevice device, uint32_t queue_family_index, bool transient, bool resettable, uint32_t count, VkCommandPool* out_pools)
{
    for(uint32_t i = 0; i < count; i++)
    {
        vk_cmd_create_pool(device, queue_family_index, transient, resettable, &out_pools[i]);
    }
}

void vk_cmd_destroy_many_pools(VkDevice device, uint32_t count, VkCommandPool* pools)
{
    for(uint32_t i = 0; i < count; i++)
        vk_cmd_destroy_pool(device, pools[i]);
}

void vk_cmd_alloc(VkDevice device, VkCommandPool pool, bool primary, VkCommandBuffer* out_cmd)
{
    VkCommandBufferAllocateInfo ci = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool,
        .level              = vk_cmd_level(primary),
        .commandBufferCount = 1,
    };

    VK_CHECK(vkAllocateCommandBuffers(device, &ci, out_cmd));
}


void vk_cmd_begin(VkCommandBuffer cmd, bool one_time)
{

    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = one_time ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0u,
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
}

void vk_cmd_end(VkCommandBuffer cmd)
{
    VK_CHECK(vkEndCommandBuffer(cmd));
}

void vk_cmd_submit_once(VkDevice device, VkQueue queue, VkCommandBuffer cmd)
{
    VkSubmitInfo submit = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd,
    };

    VkFenceCreateInfo fc = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    VkFence fence = VK_NULL_HANDLE;

    VK_CHECK(vkCreateFence(device, &fc, NULL, &fence));
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(device, fence, NULL);
}

void vk_cmd_reset(VkCommandBuffer cmd)
{
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
}

void vk_cmd_reset_pool(VkDevice device, VkCommandPool pool)
{
    VK_CHECK(vkResetCommandPool(device, pool, 0));
}


void end_one_time_cmd(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd)
{
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo = {
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };

    VkSubmitInfo2 submit = {
        .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos    = &cmdInfo,
    };

    VK_CHECK(vkQueueSubmit2(queue, 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    vkFreeCommandBuffers(device, pool, 1, &cmd);
}
VkCommandBuffer begin_one_time_cmd(VkDevice device, VkCommandPool pool)
{
    VkCommandBuffer cmd = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
//    render_reset_state();
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    return cmd;
}




uint32_t hash32_bytes(const void* data, size_t size)
{
    return (uint32_t)XXH32(data, size, 0);
}

uint64_t hash64_bytes(const void* data, size_t size)
{
    return (uint64_t)XXH64(data, size, 0);
}

uint32_t round_up(uint32_t a, uint32_t b)
{
    return (a + b - 1) & ~(b - 1);
}
uint64_t round_up_64(uint64_t a, uint64_t b)
{
    return (a + b - 1) & ~(b - 1);
}

size_t c99_strnlen(const char* s, size_t maxlen)
{
    size_t i = 0;
    if(!s)
        return 0;
    for(; i < maxlen && s[i]; i++)
    {
    }
    return i;
}
