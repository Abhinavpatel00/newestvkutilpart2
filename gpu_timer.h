
#pragma once
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef GPU_PROF_MAX_SCOPES
#define GPU_PROF_MAX_SCOPES 128
#endif

#ifndef GPU_PROF_NAME_MAX
#define GPU_PROF_NAME_MAX 32
#endif

typedef struct GpuScope
{
    char name[GPU_PROF_NAME_MAX];

    uint32_t begin;
    uint32_t end;

    uint32_t depth;

} GpuScope;

typedef struct GpuResolvedScope
{
    char     name[GPU_PROF_NAME_MAX];
    float    us;
    uint32_t depth;

} GpuResolvedScope;

typedef struct GpuProfiler
{
    VkDevice    device;
    VkQueryPool pool;

    float timestamp_period_ns;

    uint32_t capacity;
    uint32_t cursor;

    GpuScope scopes[GPU_PROF_MAX_SCOPES];
    uint32_t scope_count;

    uint32_t stack[GPU_PROF_MAX_SCOPES];
    uint32_t stack_top;

    uint64_t timestamps[GPU_PROF_MAX_SCOPES * 2];

    GpuResolvedScope resolved[GPU_PROF_MAX_SCOPES];
    uint32_t         resolved_count;

} GpuProfiler;


bool gpu_prof_init(GpuProfiler* p, VkDevice device, VkPhysicalDevice gpu, uint32_t max_queries);

void gpu_prof_destroy(GpuProfiler* p);

void gpu_prof_begin_frame(VkCommandBuffer cmd, GpuProfiler* p);

void gpu_prof_end_frame(VkCommandBuffer cmd, GpuProfiler* p);

void gpu_prof_scope_begin(VkCommandBuffer cmd, GpuProfiler* p, const char* name, VkPipelineStageFlags2 stage);

void gpu_prof_scope_end(VkCommandBuffer cmd, GpuProfiler* p, VkPipelineStageFlags2 stage);

void gpu_prof_resolve(GpuProfiler* p);

bool gpu_prof_get_us(GpuProfiler* p, const char* name, float* out_us);

void gpu_prof_dump(GpuProfiler* p);


#define GPU_SCOPE(cmd, prof, name, stage)                                                                              \
    for(int _once = (gpu_prof_scope_begin(cmd, prof, name, stage), 0); !_once; gpu_prof_scope_end(cmd, prof, stage), _once = 1)
