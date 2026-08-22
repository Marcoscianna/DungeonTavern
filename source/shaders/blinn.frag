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

// UBO Globale
layout(binding = 0, set = 0) uniform GlobalUniformBufferObject {
    vec3 lightDir;
    vec4 lightColor;
    vec3 eyePos;
    mat4 lightVP;
    PointLight pLights[50];
    int numLights;
} gubo;

// La mappa delle ombre generata nel Pass 0
layout(binding = 1, set = 0) uniform sampler2D shadowMap;

void main() {
    vec3 N = normalize(fragNorm);
    vec3 albedo = pow(texture(albedoMap, fragUV).rgb, vec3(2.2));
    vec3 V = normalize(gubo.eyePos - fragPos);

    // ==========================================
    // 1. LUCE DIREZIONALE (Sole/Luna)
    // ==========================================
    vec3 L = normalize(-gubo.lightDir);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float HdotN = max(dot(H, N), 0.0);

    float specularStrength = 0.02;
    vec3 specular = vec3(0.0);

    if (NdotL > 0.0) {
        // Esponente 128.0 restringe il riflesso, togliendo l'effetto "cono gigante"
        specular = vec3(pow(HdotN, 128.0)) * specularStrength;
    }

    // --- CALCOLO OMBRA (Shadow Mapping) ---
    vec4 lightSpacePos = gubo.lightVP * vec4(fragPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float shadow = 1.0;
    if(projCoords.z > -1.0 && projCoords.z < 1.0) {
        float closestDepth = texture(shadowMap, projCoords.xy).r;
        float bias = max(0.005 * (1.0 - NdotL), 0.001);
        if(projCoords.z - bias > closestDepth) {
            shadow = 0.0; // In ombra
        }
    }

    vec3 finalLight = (albedo * NdotL + specular) * gubo.lightColor.rgb * shadow;

    // ==========================================
    // 2. LUCI PUNTIFORMI (Fuoco e Candele)
    // ==========================================
    for(int i = 0; i < gubo.numLights; i++) {
        vec3 lightVec = gubo.pLights[i].position - fragPos;
        float distance = length(lightVec);
        vec3 L_pt = normalize(lightVec);
        vec3 H_pt = normalize(V + L_pt);

        float yDistance = abs(gubo.pLights[i].position.y - fragPos.y);
        float verticalCutoff = clamp(1.0 - (yDistance / 3.0), 0.0, 1.0);

        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));
        attenuation *= verticalCutoff;

        float NdotL_pt = max(dot(N, L_pt), 0.0);
        float HdotN_pt = max(dot(H_pt, N), 0.0);

        vec3 specular_pt = vec3(0.0);
        if (NdotL_pt > 0.0) {
            specular_pt = vec3(pow(HdotN_pt, 128.0)) * specularStrength;
        }

        finalLight += (albedo * NdotL_pt + specular_pt) * gubo.pLights[i].color * attenuation;
    }

    // ==========================================
    // 3. COMPOSIZIONE FINALE
    // ==========================================
    vec3 ambient = 0.02 * albedo;
    vec3 color = ambient + finalLight;

    // Tone mapping e Gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}