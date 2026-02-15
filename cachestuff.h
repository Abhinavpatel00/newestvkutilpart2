
#pragma once
#include "vk_default.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_BINDINGS_PER_SET 32
#define MAX_DESCRIPTOR_LAYOUTS 256

#define MAX_PIPELINE_LAYOUTS 256
#define VK_MAX_PIPELINE_SETS 8
#define VK_MAX_PUSH_RANGES 8


typedef struct DescriptorLayoutKey
{
    uint32_t binding_count;

    VkDescriptorSetLayoutBinding bindings[MAX_BINDINGS_PER_SET];

    VkDescriptorSetLayoutCreateFlags create_flags;

    VkDescriptorBindingFlags binding_flags[MAX_BINDINGS_PER_SET];

    Hash64 hash;

} DescriptorLayoutKey;


typedef struct DescriptorLayoutEntry
{
    DescriptorLayoutKey   key;
    VkDescriptorSetLayout layout;

} DescriptorLayoutEntry;


typedef struct DescriptorLayoutCache
{
    DescriptorLayoutEntry entries[MAX_DESCRIPTOR_LAYOUTS];
    uint32_t              count;

} DescriptorLayoutCache;


void descriptor_layout_cache_init(DescriptorLayoutCache* cache);

void descriptor_layout_cache_destroy(DescriptorLayoutCache* cache, VkDevice device);

VkDescriptorSetLayout descriptor_layout_cache_get(DescriptorLayoutCache*              cache,
                                                  VkDevice                            device,
                                                  const VkDescriptorSetLayoutBinding* bindings,
                                                  uint32_t                            binding_count,
                                                  VkDescriptorSetLayoutCreateFlags    create_flags,
                                                  const VkDescriptorBindingFlags*     binding_flags);


//
// Pipeline layout cache
//

typedef struct PipelineLayoutKey
{
    VkDescriptorSetLayout set_layouts[VK_MAX_PIPELINE_SETS];
    uint32_t              set_layout_count;

    VkPushConstantRange push_constants[VK_MAX_PUSH_RANGES];
    uint32_t            push_constant_count;

    Hash64 hash;

} PipelineLayoutKey;


typedef struct PipelineLayoutEntry
{
    PipelineLayoutKey key;
    VkPipelineLayout  layout;

} PipelineLayoutEntry;


typedef struct PipelineLayoutCache
{
    PipelineLayoutEntry entries[MAX_PIPELINE_LAYOUTS];
    uint32_t            count;

} PipelineLayoutCache;


void pipeline_layout_cache_init(PipelineLayoutCache* cache);

void pipeline_layout_cache_destroy(VkDevice device, PipelineLayoutCache* cache);

VkPipelineLayout pipeline_layout_cache_get(VkDevice                     device,
                                           PipelineLayoutCache*         cache,
                                           const VkDescriptorSetLayout* set_layouts,
                                           uint32_t                     set_layout_count,
                                           const VkPushConstantRange*   push_ranges,
                                           uint32_t                     push_range_count);


VkPipelineLayout pipeline_layout_cache_build(VkDevice                                   device,
                                             DescriptorLayoutCache*                     desc_cache,
                                             PipelineLayoutCache*                       pipe_cache,
                                             const VkDescriptorSetLayoutBinding* const* set_bindings,
                                             const uint32_t*                            binding_counts,
                                             const VkDescriptorSetLayoutCreateFlags*    set_create_flags,
                                             const VkDescriptorBindingFlags* const*     set_binding_flags,
                                             uint32_t                                   set_count,
                                             const VkPushConstantRange*                 push_ranges,
                                             uint32_t                                   push_count);


//
// External hash function
//

Hash64 hash64_bytes(const void* data, size_t size);
