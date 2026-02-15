
#include "cachestuff.h"
#include <string.h>
#include <assert.h>
#include <xxhash.h>

//
// internal helpers
//

static void sort_bindings_in_place(VkDescriptorSetLayoutBinding* bindings, VkDescriptorBindingFlags* flags, uint32_t count)
{
    for(uint32_t i = 0; i < count; i++)
    {
        for(uint32_t j = i + 1; j < count; j++)
        {
            if(bindings[j].binding < bindings[i].binding)
            {
                VkDescriptorSetLayoutBinding tb = bindings[i];
                bindings[i]                     = bindings[j];
                bindings[j]                     = tb;

                VkDescriptorBindingFlags tf = flags[i];
                flags[i]                    = flags[j];
                flags[j]                    = tf;
            }
        }
    }
}

static uint64_t hash_layout_key(DescriptorLayoutKey* k)
{
    XXH64_state_t* s = XXH64_createState();
    XXH64_reset(s, 0);

    XXH64_update(s, &k->binding_count, sizeof(k->binding_count));
    XXH64_update(s, &k->create_flags, sizeof(k->create_flags));

    if(k->binding_count)
    {
        XXH64_update(s, k->bindings, sizeof(VkDescriptorSetLayoutBinding) * k->binding_count);

        XXH64_update(s, k->binding_flags, sizeof(VkDescriptorBindingFlags) * k->binding_count);
    }

    uint64_t hash = XXH64_digest(s);
    XXH64_freeState(s);

    return hash;
}
static bool layout_key_equals(const DescriptorLayoutKey* a, const DescriptorLayoutKey* b)
{
    if(a->hash != b->hash)
        return false;

    if(a->binding_count != b->binding_count)
        return false;

    if(a->create_flags != b->create_flags)
        return false;

    if(a->binding_count == 0)
        return true;

    if(memcmp(a->bindings, b->bindings, sizeof(VkDescriptorSetLayoutBinding) * a->binding_count) != 0)
        return false;

    if(memcmp(a->binding_flags, b->binding_flags, sizeof(VkDescriptorBindingFlags) * a->binding_count) != 0)
        return false;

    return true;
}


//
// descriptor cache implementation
//

void descriptor_layout_cache_init(DescriptorLayoutCache* cache)
{
    cache->count = 0;
}


void descriptor_layout_cache_destroy(DescriptorLayoutCache* cache, VkDevice device)
{
    for(uint32_t i = 0; i < cache->count; i++)
    {
        vkDestroyDescriptorSetLayout(device, cache->entries[i].layout, NULL);
    }

    cache->count = 0;
}


VkDescriptorSetLayout descriptor_layout_cache_get(DescriptorLayoutCache*              cache,
                                                  VkDevice                            device,
                                                  const VkDescriptorSetLayoutBinding* bindings,
                                                  uint32_t                            binding_count,
                                                  VkDescriptorSetLayoutCreateFlags    create_flags,
                                                  const VkDescriptorBindingFlags*     binding_flags)
{
    DescriptorLayoutKey key = {0};

    key.binding_count = binding_count;
    key.create_flags  = create_flags;

    for(uint32_t i = 0; i < binding_count; i++)
    {
        key.bindings[i]      = bindings[i];
        key.binding_flags[i] = binding_flags ? binding_flags[i] : 0;
    }

    sort_bindings_in_place(key.bindings, key.binding_flags, binding_count);

    key.hash = hash_layout_key(&key);

    for(uint32_t i = 0; i < cache->count; i++)
    {
        if(layout_key_equals(&cache->entries[i].key, &key))
        {
            return cache->entries[i].layout;
        }
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,

                                                            .bindingCount  = binding_count,
                                                            .pBindingFlags = key.binding_flags};

    VkDescriptorSetLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,

                                          .pNext = binding_flags ? &flags_ci : NULL,

                                          .flags        = create_flags,
                                          .bindingCount = binding_count,
                                          .pBindings    = key.bindings};

    VkDescriptorSetLayout layout;

    vkCreateDescriptorSetLayout(device, &ci, NULL, &layout);

    cache->entries[cache->count++] = (DescriptorLayoutEntry){.key = key, .layout = layout};

    return layout;
}

//
// pipeline layout cache
//

static Hash64 hash_pipeline_layout_key(const PipelineLayoutKey* k)
{
    XXH64_state_t* s = XXH64_createState();
    XXH64_reset(s, 0);

    XXH64_update(s, k->set_layouts, sizeof(VkDescriptorSetLayout) * k->set_layout_count);

    XXH64_update(s, k->push_constants, sizeof(VkPushConstantRange) * k->push_constant_count);
    uint64_t hash = XXH64_digest(s);
    XXH64_freeState(s);
    return hash;
}


void pipeline_layout_cache_init(PipelineLayoutCache* cache)
{
    cache->count = 0;
}


void pipeline_layout_cache_destroy(VkDevice device, PipelineLayoutCache* cache)
{
    for(uint32_t i = 0; i < cache->count; i++)
    {
        vkDestroyPipelineLayout(device, cache->entries[i].layout, NULL);
    }

    cache->count = 0;
}


VkPipelineLayout pipeline_layout_cache_get(VkDevice                     device,
                                           PipelineLayoutCache*         cache,
                                           const VkDescriptorSetLayout* set_layouts,
                                           uint32_t                     set_layout_count,
                                           const VkPushConstantRange*   push_ranges,
                                           uint32_t                     push_range_count)
{
    PipelineLayoutKey key = {0};

    key.set_layout_count    = set_layout_count;
    key.push_constant_count = push_range_count;

    memcpy(key.set_layouts, set_layouts, sizeof(VkDescriptorSetLayout) * set_layout_count);

    memcpy(key.push_constants, push_ranges, sizeof(VkPushConstantRange) * push_range_count);

    key.hash = hash_pipeline_layout_key(&key);

    for(uint32_t i = 0; i < cache->count; i++)
    {
        if(cache->entries[i].key.hash == key.hash)
            return cache->entries[i].layout;
    }

    VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,

                                     .setLayoutCount = set_layout_count,
                                     .pSetLayouts    = set_layouts,

                                     .pushConstantRangeCount = push_range_count,

                                     .pPushConstantRanges = push_ranges};

    VkPipelineLayout layout;

    vkCreatePipelineLayout(device, &ci, NULL, &layout);

    cache->entries[cache->count++] = (PipelineLayoutEntry){.key = key, .layout = layout};

    return layout;
}


VkPipelineLayout pipeline_layout_cache_build(VkDevice                                   device,
                                             DescriptorLayoutCache*                     desc_cache,
                                             PipelineLayoutCache*                       pipe_cache,
                                             const VkDescriptorSetLayoutBinding* const* set_bindings,
                                             const uint32_t*                            binding_counts,
                                             const VkDescriptorSetLayoutCreateFlags*    set_create_flags,
                                             const VkDescriptorBindingFlags* const*     set_binding_flags,
                                             uint32_t                                   set_count,
                                             const VkPushConstantRange*                 push_ranges,
                                             uint32_t                                   push_count)
{
    VkDescriptorSetLayout layouts[VK_MAX_PIPELINE_SETS] = {0};
    for(uint32_t i = 0; i < set_count; i++)
    {
        VkDescriptorSetLayoutCreateFlags flags = 0;
        if(set_create_flags)
            flags = set_create_flags[i];
        const VkDescriptorBindingFlags* bflags = NULL;
        if(set_binding_flags)
            bflags = set_binding_flags[i];
        layouts[i] = descriptor_layout_cache_get(desc_cache, device, set_bindings[i], binding_counts[i], flags, bflags);
    }
    return pipeline_layout_cache_get(device, pipe_cache, layouts, set_count, push_ranges, push_count);
}
