#version 450

// --- INPUT DEL VERTICE ---
// Devono combaciare esattamente con i "location" dichiarati nel tuo VDanim
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inWeights;
layout(location = 4) in uvec4 inJoints;

// --- UNIFORM BUFFERS ---
// Set 1, Binding 0: Il tuo AnimUniformBufferObject
layout(set = 1, binding = 0) uniform AnimUniformBufferObject {
    mat4 mvpMat;
    mat4 mMat;
    mat4 bones[128]; // Array delle matrici delle ossa
} ubo;

// --- OUTPUT VERSO IL FRAGMENT SHADER ---
// Passiamo questi dati al "toChangeBlinnFromPos.frag"
layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNorm;

void main() {
    // 1. SKINNING: Calcola la matrice di trasformazione combinata delle ossa per questo vertice
    // Moltiplica la matrice dell'osso (bones) per l'indice corrispondente (inJoints), scalata per il suo peso (inWeights)
    mat4 boneTransform = ubo.bones[int(inJoints.x)] * inWeights.x;
    boneTransform     += ubo.bones[int(inJoints.y)] * inWeights.y;
    boneTransform     += ubo.bones[int(inJoints.z)] * inWeights.z;
    boneTransform     += ubo.bones[int(inJoints.w)] * inWeights.w;

    // 2. APPLICA LA DEFORMAZIONE AL VERTICE
    // Moltiplica la posizione originale per la matrice delle ossa appena calcolata
    vec4 skinnedPosition = boneTransform * vec4(inPosition, 1.0);

    // 3. PROIEZIONE SU SCHERMO
    // Moltiplica per Model-View-Projection per posizionarlo nella telecamera
    gl_Position = ubo.mvpMat * skinnedPosition;

    // 4. DATI PER IL FRAGMENT SHADER (Illuminazione e Texture)
    // Posizione del vertice nel mondo
    fragPos = vec3(ubo.mMat * skinnedPosition);

    // Ricalcola le normali affinché la luce segua la deformazione delle ossa
    mat4 normalMatrix = ubo.mMat * boneTransform;
    fragNorm = normalize(mat3(normalMatrix) * inNormal);

    // Passa le coordinate della texture inalterate
    fragUV = inUV;
}