#version 450
#extension GL_ARB_separate_shader_objects : enable

struct PointLight {
    vec3 position;
    vec3 color;
};

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNorm;

layout(location = 0) out vec4 outColor;
layout(binding = 1, set = 1) uniform sampler2D albedoMap;

layout(binding = 0, set = 0) uniform GlobalUniformBufferObject {
    vec3 lightDir;
    vec4 lightColor;
    vec3 eyePos;
    PointLight pLights[5];
    int numLights;
} gubo;

void main() {
    vec3 N = normalize(fragNorm);
    vec3 albedo = pow(texture(albedoMap, fragUV).rgb, vec3(2.2));
    vec3 V = normalize(gubo.eyePos - fragPos);

    // --- LUCE DIREZIONALE (Sole/Luna) ---
    vec3 L = normalize(-gubo.lightDir);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float HdotN = max(dot(H, N), 0.0);

    float specularStrength = 0.01; // Riflessi moderati
    vec3 specular = vec3(pow(HdotN, 64.0)) * specularStrength;
    vec3 finalLight = (albedo * NdotL + specular) * gubo.lightColor.rgb;

    // --- LUCI PUNTIFORMI (Fuoco e Candele) ---
    for(int i = 0; i < gubo.numLights; i++) {
        vec3 lightVec = gubo.pLights[i].position - fragPos;
        float distance = length(lightVec);
        vec3 L_pt = normalize(lightVec);
        vec3 H_pt = normalize(V + L_pt);

        // Formula di attenuazione fisica della luce
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));

        float NdotL_pt = max(dot(N, L_pt), 0.0);
        float HdotN_pt = max(dot(H_pt, N), 0.0);

        vec3 specular_pt = vec3(pow(HdotN_pt, 64.0)) * specularStrength;
        finalLight += (albedo * NdotL_pt + specular_pt) * gubo.pLights[i].color * attenuation;
    }

    vec3 ambient = 0.02 * albedo;
    vec3 color = ambient + finalLight;

    // Tone mapping e Gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}