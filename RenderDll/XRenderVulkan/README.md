# Vulkan Renderer Status

**Current State:** Unusable / Work-In-Progress

The Vulkan renderer implementation in this directory is a skeleton and is not yet functional for gameplay.

## Missing Features

1.  **Shaders:** No system to load or compile engine shaders (HLSL/CG) to SPIR-V. Internal shaders are placeholders.
2.  **Pipeline:** Vertex input attributes are not configured (`vertexBindingDescriptionCount = 0`), so the GPU cannot read mesh data.
3.  **Textures:** Texture loading is implemented, but texture binding (Descriptor Sets) is unimplemented.
4.  **Lighting & Shadows:** All lighting and shadow functions are empty stubs.
5.  **Transformations:** Matrix stack (Rotate, Translate) is incomplete.
6.  **Performance:** Uses inefficient immediate mode emulation for drawing.

## Next Steps for Implementation

1.  Integrate a shader compiler (e.g., glslang or shaderc) to cross-compile engine shaders to SPIR-V.
2.  Implement `CVulkanPipeline::CreatePipeline` to correctly map `m_VertexFormat` to `VkVertexInputAttributeDescription`.
3.  Implement Descriptor Set management in `CVulkanPipeline` and `CVulkanRenderer` to bind textures and uniform buffers.
4.  Implement Uniform Buffer Objects (UBOs) for passing frame/object constants (MVP, light data).
5.  Flesh out `Draw3dBBox` and other culling/visibility functions.
