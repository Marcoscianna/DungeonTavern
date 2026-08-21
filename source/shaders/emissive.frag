#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNorm; // Non ci serve la luce, ma deve esserci per matchare static.vert

layout(location = 0) out vec4 outColor;
layout(binding = 1, set = 1) uniform sampler2D albedoMap;

void main() {
    vec3 albedo = texture(albedoMap, fragUV).rgb;

    vec3 emissive = albedo * 5.0;

    outColor = vec4(emissive, 1.0);
}