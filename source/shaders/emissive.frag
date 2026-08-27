#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNorm;

layout(location = 0) out vec4 outColor;

layout(binding = 1, set = 1) uniform sampler2D albedoMap;
layout(binding = 2, set = 1) uniform sampler2D emissiveMap;

void main() {
    // 1. Campioniamo l'intero vec4 per avere anche l'alpha del PNG
    vec4 albedo = texture(albedoMap, fragUV);
    vec4 emission = texture(emissiveMap, fragUV);

    // 2. Calcoliamo il livello di luminosità di entrambe le immagini
    float brightAlbedo = max(albedo.r, max(albedo.g, albedo.b));
    float brightEmission = max(emission.r, max(emission.g, emission.b));

    // 3. DISCARD AGGRESSIVO:
    // Buttiamo via il pixel se:
    // - L'albedo è nero (brightAlbedo < 0.1)
    // - L'emissione è nera (brightEmission < 0.1)
    // - Il PNG ha lo sfondo trasparente (albedo.a < 0.1)
    if (brightAlbedo < 0.1 || brightEmission < 0.1 || albedo.a < 0.1) {
        discard;
    }

    // 4. Manteniamo solo il colore puro dell'emissive amplificato
    vec3 finalColor = emission.rgb * 2.5;

    outColor = vec4(finalColor, 1.0);
}