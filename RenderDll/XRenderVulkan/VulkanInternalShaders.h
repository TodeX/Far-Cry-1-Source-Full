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

// --------------------------------------------------------------------------------
// General 3D Shaders (VS_General, PS_General)
// --------------------------------------------------------------------------------

/*
// VS_General.vert
#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 MVP;
} pushConstants;

void main() {
    gl_Position = pushConstants.MVP * vec4(inPosition, 1.0);
    outColor = inColor;
    outTexCoord = inTexCoord;
}
*/
const uint32_t VS_General[] = {
    0x07230203, 0x00010000, 0x00080001, 0x00000018, 0x00000000 // Placeholder header
};

/*
// PS_General.frag
#version 450
layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outFragColor;

void main() {
    outFragColor = vec4(1.0, 1.0, 1.0, 1.0); // Solid white for wireframe
}
*/
const uint32_t PS_General[] = {
    0x07230203, 0x00010000, 0x00080001, 0x00000018, 0x00000000 // Placeholder header
};

#endif
