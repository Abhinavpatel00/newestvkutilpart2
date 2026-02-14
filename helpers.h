#include "vk_default.h"
/* ------------------ Fence helpers ------------------ */

void vk_create_fence(VkDevice device, bool signaled, VkFence* out_fence);
void vk_create_fences(VkDevice device, uint32_t count, bool signaled, VkFence* out_fences);

void vk_wait_fence(VkDevice device, VkFence fence, uint64_t timeout_ns);
void vk_wait_fences(VkDevice device, uint32_t count, const VkFence* fences, bool wait_all, uint64_t timeout_ns);

void vk_reset_fence(VkDevice device, VkFence fence);
void vk_reset_fences(VkDevice device, uint32_t count, const VkFence* fences);

bool vk_fence_is_signaled(VkDevice device, VkFence fence);

void vk_destroy_fences(VkDevice device, uint32_t count, VkFence* fences);

/* ------------------ Semaphore helpers ------------------ */

void vk_create_semaphore(VkDevice device, VkSemaphore* out_semaphore);
void vk_create_semaphores(VkDevice device, uint32_t count, VkSemaphore* out_semaphores);

void vk_destroy_semaphores(VkDevice device, uint32_t count, VkSemaphore* semaphores);



/* single pool */
void vk_cmd_create_pool(VkDevice device, uint32_t queue_family_index, bool transient, bool resettable, VkCommandPool* out_pool);

void vk_cmd_destroy_pool(VkDevice device, VkCommandPool pool);


/* allocation */
void vk_cmd_alloc(VkDevice device, VkCommandPool pool, bool primary, VkCommandBuffer* out_cmd);


/* recording */
void vk_cmd_begin(VkCommandBuffer cmd, bool one_time);
void vk_cmd_end(VkCommandBuffer cmd);

/* submit and wait */
void vk_cmd_submit_once(VkDevice device, VkQueue queue, VkCommandBuffer cmd);

/* resets */
void vk_cmd_reset(VkCommandBuffer cmd);
void vk_cmd_reset_pool(VkDevice device, VkCommandPool pool);

VkCommandBuffer begin_one_time_cmd(VkDevice device, VkCommandPool pool);


void end_one_time_cmd(VkDevice device, VkQueue queue, VkCommandPool pool, VkCommandBuffer cmd);


