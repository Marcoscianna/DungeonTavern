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
    mat4 lightVP;
    PointLight pLights[50];
    int numLights;
    float shadowToggle;
} gubo;

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

    float specularStrength = 0.8;
    vec3 specular_dir = vec3(0.0);
    if (NdotL > 0.0) {
        // Esponente 256.0 = riflesso concentrato
        // Moltiplichiamo per albedo per colorare il riflesso
        specular_dir = albedo * pow(HdotN, 256.0) * specularStrength;
    }

// --- CALCOLO OMBRA CON PCF (Soft Shadows) ---
    float shadow = 1.0;
    if(gubo.shadowToggle > 0.0) {
        vec4 lightSpacePos = gubo.lightVP * vec4(fragPos, 1.0);
        vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
        projCoords.xy = projCoords.xy * 0.5 + 0.5;

        shadow = 1.0;

        if(projCoords.z > -1.0 && projCoords.z < 1.0 &&
           projCoords.x > 0.0 && projCoords.x < 1.0 &&
           projCoords.y > 0.0 && projCoords.y < 1.0) {

            float bias = max(0.015 * (1.0 - NdotL), 0.005);

            vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
            float currentShadow = 0.0;

            // PCF: Campioniamo la griglia 3x3
            for(int x = -1; x <= 1; ++x) {
                for(int y = -1; y <= 1; ++y) {
                    float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                    if(projCoords.z - bias > pcfDepth) {
                        currentShadow += 1.0;
                    }
                }
            }
            shadow = 1.0 - (currentShadow / 9.0);
        }
    }
    //shadow = mix(1.0, shadow, gubo.shadowToggle);
    vec3 finalLight = (albedo * NdotL + specular_dir) * gubo.lightColor.rgb * shadow;

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

        float attenuation = 1.0 / (6.0*(1.0 + 0.09 * distance + 0.032 * (distance * distance)));
        attenuation *= verticalCutoff;

        float NdotL_pt = max(dot(N, L_pt), 0.0);
        float HdotN_pt = max(dot(H_pt, N), 0.0);

        vec3 specular_pt = vec3(0.0);
        if (NdotL_pt > 0.0) {
           specular_pt = albedo * pow(HdotN_pt, 256.0) * (specularStrength * 1.5);
        }

        finalLight += (albedo * NdotL_pt + specular_pt) * gubo.pLights[i].color * attenuation;
    }

    // ==========================================
    // 3. COMPOSIZIONE FINALE
    // ==========================================
    vec3 ambient = vec3(0.005) * albedo;
    vec3 color = ambient + finalLight;

    // Tone mapping e Gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}