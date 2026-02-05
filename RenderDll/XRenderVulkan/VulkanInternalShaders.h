#ifndef VULKAN_INTERNAL_SHADERS_H
#define VULKAN_INTERNAL_SHADERS_H

#include <stdint.h>

// These arrays should contain compiled SPIR-V bytecode for internal shaders.
// They can be generated using glslc or glslangValidator.
// Example: glslc shader.vert -o shader.vert.spv
//          xxd -i shader.vert.spv > shader_vert.h

// Vertex Shader for 2D Image drawing
// Inputs: Position (vec3), Color (uvec4), TexCoord (vec2)
// Push Constant: MVP Matrix (mat4)
// Output: Color (vec4), TexCoord (vec2)
const uint32_t VS_2D[] = {
    0x07230203, 0x00010000, 0x00080001, 0x00000018, 0x00000000 // Placeholder header
};

// Fragment Shader for 2D Image drawing
// Inputs: Color (vec4), TexCoord (vec2)
// Uniform: Texture Sampler
// Output: Color (vec4)
const uint32_t PS_2D[] = {
    0x07230203, 0x00010000, 0x00080001, 0x00000018, 0x00000000 // Placeholder header
};

#endif
