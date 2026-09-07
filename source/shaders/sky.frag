#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNorm;

layout(location = 0) out vec4 outColor;

layout(binding = 1, set = 1) uniform sampler2D albedoMap;
layout(binding = 2, set = 1) uniform sampler2D emissiveMap;

void main() {
    vec4 albedo = texture(albedoMap, fragUV);
    vec4 emission = texture(emissiveMap, fragUV);

    if (albedo.a < 0.1) {
        discard;
    }

    // 1. Sommiamo il colore base all'emissione 
    vec3 hdrColor = albedo.rgb + (emission.rgb * 1.5);

    // 2. Mappatura dell'esposizione: preserva la saturazione dei rossi/arancioni
    float exposure = 1.0;
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);

    outColor = vec4(mapped, 1.0);
}