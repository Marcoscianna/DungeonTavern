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

// Texture binding per il materiale (Diffuse, Normal, Specular)
layout(binding = 1, set = 1) uniform sampler2D diffuseMap;
layout(binding = 2, set = 1) uniform sampler2D normalMap;
layout(binding = 3, set = 1) uniform sampler2D specularMap;

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

// Calcolo dinamico della matrice TBN (Tangent-Bitangent-Normal) in screen-space.
// Sfruttando le derivate parziali dFdx/dFdy evitiamo di dover pre-calcolare
// e passare i vettori Tangente dal C++ e dal Vertex Shader, risparmiando memoria.
mat3 calculateTBN(vec3 N, vec3 p, vec2 uv) {
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);

    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

void main() {
    vec3 baseNorm = normalize(fragNorm);
    mat3 TBN = calculateTBN(baseNorm, fragPos, fragUV);

    // ==========================================
    // NORMAL E SPECULAR MAPPING
    // ==========================================

    // Mappatura delle normali da [0, 1] a [-1, 1] e allineamento con la superficie tramite TBN
    vec3 normalSample = texture(normalMap, fragUV).rgb;
    normalSample = normalSample * 2.0 - 1.0;
    vec3 N = normalize(TBN * normalSample);

    // Decodifica dell'albedo per lavorare nello spazio lineare
    vec3 albedo = pow(texture(diffuseMap, fragUV).rgb, vec3(2.2));

    // Estrazione dell'intensità speculare (essendo in scala di grigi, ci basta il canale rosso)
    float specularStrength = texture(specularMap, fragUV).r;

    vec3 V = normalize(gubo.eyePos - fragPos);

    // ==========================================
    // CALCOLO OMBRE (DIRECTIONAL LIGHT)
    // ==========================================
    float shadow = 1.0;
    if(gubo.shadowToggle > 0.0) {
        // Proiezione del frammento nello spazio della telecamera del sole (Shadow Map)
        vec4 lightSpacePos = gubo.lightVP * vec4(fragPos, 1.0);
        vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
        projCoords.xy = projCoords.xy * 0.5 + 0.5;

        // Limita il calcolo dell'ombra all'interno del frustum visibile della luce
        if(projCoords.z > -1.0 && projCoords.z < 1.0 && projCoords.x > 0.0 && projCoords.x < 1.0 && projCoords.y > 0.0 && projCoords.y < 1.0) {
            // Bias adattivo per combattere lo "Shadow Acne" dipendente dall'inclinazione della superficie
            float bias = max(0.015 * (1.0 - max(dot(N, normalize(-gubo.lightDir)), 0.0)), 0.005);
            vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
            float currentShadow = 0.0;

            // Filtro PCF (Percentage-Closer Filtering) 3x3 per ammorbidire visivamente i bordi dell'ombra
            for(int x = -1; x <= 1; ++x) {
                for(int y = -1; y <= 1; ++y) {
                    float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                    if(projCoords.z - bias > pcfDepth) currentShadow += 1.0;
                }
            }
            shadow = 1.0 - (currentShadow / 9.0);
        }
    }

    // ==========================================
    // RISOLUZIONE LUCE DIREZIONALE (BLINN-PHONG)
    // ==========================================
    vec3 L_dir = normalize(-gubo.lightDir);
    vec3 H_dir = normalize(V + L_dir);
    float NdotL_dir = max(dot(N, L_dir), 0.0);
    float HdotN_dir = max(dot(H_dir, N), 0.0);

    vec3 specular_dir = vec3(0.0);
    if (NdotL_dir > 0.0) {
        specular_dir = vec3(pow(HdotN_dir, 64.0)) * specularStrength;
    }

    vec3 finalLight = (albedo * NdotL_dir + specular_dir) * gubo.lightColor.rgb * shadow;

    // ==========================================
    // RISOLUZIONE LUCI PUNTIFORMI
    // ==========================================
    for(int i = 0; i < gubo.numLights; i++) {
        vec3 lightVec = gubo.pLights[i].position - fragPos;
        float distance = length(lightVec);
        vec3 L_pt = normalize(lightVec);
        vec3 H_pt = normalize(V + L_pt);

        // Attenuazione quadratica basata sulla distanza fisica
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));

        // Smorzamento aggiuntivo sull'asse Y per confinare la luce ai piani orizzontali
        float yDistance = abs(gubo.pLights[i].position.y - fragPos.y);
        attenuation *= clamp(1.0 - (yDistance / 3.0), 0.0, 1.0);

        float NdotL_pt = max(dot(N, L_pt), 0.0);
        float HdotN_pt = max(dot(H_pt, N), 0.0);

        vec3 specular_pt = vec3(0.0);
        if (NdotL_pt > 0.0) {
            specular_pt = vec3(pow(HdotN_pt, 64.0)) * specularStrength;
        }

        finalLight += (albedo * NdotL_pt + specular_pt) * gubo.pLights[i].color * attenuation;
    }

    // Aggiunta di una finta luce ambientale per non avere aree nere al 100%
    vec3 ambient = 0.02 * albedo;
    vec3 color = ambient + finalLight;

    // Tone mapping (Reinhard) e Gamma Correction per evitare sovraesposizione
    // e riportare i colori nello spazio sRGB
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}