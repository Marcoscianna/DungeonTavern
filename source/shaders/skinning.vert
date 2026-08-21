#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inWeights;
layout(location = 4) in uvec4 inJoints;

layout(set = 1, binding = 0) uniform AnimUniformBufferObject {
    mat4 mvpMat;
    mat4 mMat;
    mat4 bones[128];
} ubo;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNorm;

void main() {
    mat4 boneTransform = ubo.bones[int(inJoints.x)] * inWeights.x;
    boneTransform     += ubo.bones[int(inJoints.y)] * inWeights.y;
    boneTransform     += ubo.bones[int(inJoints.z)] * inWeights.z;
    boneTransform     += ubo.bones[int(inJoints.w)] * inWeights.w;

    vec4 skinnedPosition = boneTransform * vec4(inPosition, 1.0);
    gl_Position = ubo.mvpMat * skinnedPosition;

    fragPos = vec3(ubo.mMat * skinnedPosition);
    fragUV = inUV;

    mat4 normalMatrix = ubo.mMat * boneTransform;
    fragNorm = normalize(mat3(normalMatrix) * inNormal);
}