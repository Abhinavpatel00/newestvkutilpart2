# Renderer Architecture Plan (Experimental + Shippable)

## Goals
- Maximum flexibility for experimenting with many graphics algorithms.
- Stable, shippable production path with clear performance budgets.
- Fast iteration without locking into a single render strategy.

## Recommended Architecture: **Hybrid Render Graph + Feature Modules**
Use a **render graph** for frame scheduling and resource lifetime, backed by **feature modules** that can be swapped or layered (forward+, deferred, cluster, ray queries, etc.).

### Why this fits your goals
- **Experimentation:** New passes become graph nodes, easy to insert/remove.
- **Shipping:** Deterministic scheduling, explicit barriers, profiling and validation baked in.
- **Flexibility:** Multiple pipelines can coexist and share resources.

## Core Building Blocks
1. **Frame Graph (or Render Graph)**
   - DAG of passes with declared read/write resources.
   - Automatically builds barriers, transitions, and aliasing.
   - Multiple “frame recipes” (e.g., Forward+ vs. Deferred) selectable at runtime.

2. **Render Modules**
   - Each algorithm is a module with:
     - Inputs/outputs (textures/buffers)
     - Pass definitions
     - Config + runtime toggles
   - Example modules: shadow system, SSAO, SSR, TAA, bloom, GI, visibility.

3. **Data-Driven Materials + Pipelines**
   - Material templates mapping to pipeline permutations.
   - Shader variants compiled by feature toggles.
   - Pipeline cache persisted for shipping.

4. **Feature Switchboard**
   - Central configuration layer for turning modules on/off.
   - A/B testing of algorithms per frame.
   - Debug UI + console integration.

5. **GPU Resource Abstraction**
   - Resource registry + lifetime management in the graph.
   - Explicit ownership and aliasing to control memory.

6. **Profiling + Validation
**   - Per-pass GPU timers.
   - “Graph dump” to inspect schedule and barriers.
   - Vulkan validation + GPU crash markers.

## Practical Plan (Phased)
### Phase 1 — Minimal Shippable Base
- Forward+ or Deferred base path (pick one as default).
- Shadow mapping + basic post (tonemap, bloom).
- Render graph with pass/resource declarations.
- Material system + pipeline cache.

### Phase 2 — Experiment Harness
- Hot reload for shaders and config.
- UI toggles for modules.
- Graph variants (Forward+, Deferred, Hybrid).
- Automated GPU timing per pass.

### Phase 3 — Advanced Algorithms
- Add experiments as modules without touching base.
- Maintain a “stable path” configuration.
- Build data-driven benchmarks for each module.

## Minimal Interfaces (C-style)
- `render_graph_begin_frame()`
- `render_graph_add_pass()`
- `render_graph_build()`
- `render_graph_execute()`
- `renderer_register_module()`
- `renderer_set_feature_flags()`

## Guardrails for Shipping
- Lock a default graph path for release builds.
- Keep experimental modules disabled by default.
- Ensure all passes have perf budgets and fallback modes.
- CI runs validation + GPU capture tests.

## Immediate Next Steps
1. Inventory current passes in vk.c/vk.h.
2. Identify required inputs/outputs per pass.
3. Sketch your first render graph (Forward+ or Deferred).
4. Add module registration + feature flags.
5. Integrate GPU timing + simple graph debug dump.

---

## Forward+ Renderer AAPI (Slot-Based, API-Only)

### Core Types
- `renderer_ctx`
- `rg_graph`
- `rg_pass`
- `rg_handle` (texture/buffer handle)
- `rg_slot` (named input/output)
- `r_feature_flags`
- `r_frame_desc`

### Graph Lifecycle
- `renderer_init(renderer_ctx* ctx)`
- `renderer_shutdown(renderer_ctx* ctx)`
- `renderer_begin_frame(renderer_ctx* ctx, const r_frame_desc* desc)`
- `renderer_end_frame(renderer_ctx* ctx)`
- `renderer_build_graph(renderer_ctx* ctx, rg_graph* graph)`
- `renderer_execute_graph(renderer_ctx* ctx, rg_graph* graph)`

### Graph/Pass API
- `rg_graph_begin(rg_graph* graph, const char* name)`
- `rg_graph_end(rg_graph* graph)`
- `rg_add_pass(rg_graph* graph, const char* pass_name)`
- `rg_pass_set_execute(rg_pass* pass, void (*fn)(void* user))`
- `rg_pass_set_user_data(rg_pass* pass, void* user)`

### Slot API (Forward+ Friendly)
- `rg_pass_add_slot_in(rg_pass* pass, const char* slot_name, rg_handle h)`
- `rg_pass_add_slot_out(rg_pass* pass, const char* slot_name, rg_handle h)`
- `rg_pass_find_slot(rg_pass* pass, const char* slot_name)`
- `rg_slot_get_handle(const rg_slot* slot)`
- `rg_slot_set_handle(rg_slot* slot, rg_handle h)`

### Resource API
- `rg_create_texture(rg_graph* graph, const char* name, const rg_tex_desc* desc)`
- `rg_create_buffer(rg_graph* graph, const char* name, const rg_buf_desc* desc)`
- `rg_import_texture(rg_graph* graph, const char* name, vk_image image)`
- `rg_import_buffer(rg_graph* graph, const char* name, vk_buffer buffer)`
- `rg_get_blackboard(rg_graph* graph)` (global named handles)
- `rg_blackboard_set(const char* name, rg_handle h)`
- `rg_blackboard_get(const char* name)`

### Forward+ Specific Pass Slots (Suggested Names)
- `DepthPrepass`
   - in: `scene`, `camera`
   - out: `depth`
- `LightCulling`
   - in: `depth`, `camera`, `lights`
   - out: `light_grid`, `light_index_list`
- `ForwardLighting`
   - in: `scene`, `camera`, `depth`, `light_grid`, `light_index_list`
   - out: `color_hdr`, `velocity` (optional)
- `Post`
   - in: `color_hdr`
   - out: `color_ldr`

### Feature Flags
- `renderer_set_feature_flags(renderer_ctx* ctx, r_feature_flags flags)`
- `renderer_get_feature_flags(renderer_ctx* ctx)`

### Frame-Level Controls
- `renderer_set_view(renderer_ctx* ctx, const r_view_desc* view)`
- `renderer_set_scene(renderer_ctx* ctx, const r_scene_desc* scene)`
- `renderer_set_lights(renderer_ctx* ctx, const r_lights_desc* lights)`

### Debug & Profiling
- `renderer_enable_gpu_timers(renderer_ctx* ctx, bool enable)`
- `renderer_dump_graph(renderer_ctx* ctx, const char* path)`
- `renderer_get_pass_timing(renderer_ctx* ctx, const char* pass_name, r_gpu_time* out)`

### Extension Points (Algorithm Experiments)
- `renderer_register_module(renderer_ctx* ctx, const r_module_desc* module)`
- `r_module_desc::build_graph(rg_graph* graph, void* user)`
- `r_module_desc::declare_slots(rg_graph* graph, void* user)`

---

## Minimal API (Single Struct + One Push Function)

### Single Struct Declaration
- `r_instr`
   - `op` (pass opcode: `R_OP_DEPTH_PREPASS`, `R_OP_LIGHT_CULL`, `R_OP_FORWARD_LIGHTING`, `R_OP_POST`)
   - `slot_in` (named input slot)
   - `slot_out` (named output slot)
   - `params` (opaque per-pass settings blob)
   - `params_size` (bytes)

### One Push Function
- `renderer_push_instr(renderer_ctx* ctx, const r_instr* instr)`

---
If you want, I can map this directly onto your current vk.c/vk.h layout and draft the initial render-graph structs and pass registration scaffolding.
