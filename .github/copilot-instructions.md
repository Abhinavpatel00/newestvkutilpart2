# Copilot Instructions

- This renderer uses a single descriptor set layout and a single pipeline layout for all pipelines (bindless model).
- New buffers are suballocated from larger GPU buffers; pass offsets via push constants and manage ranges with the offset allocator.
