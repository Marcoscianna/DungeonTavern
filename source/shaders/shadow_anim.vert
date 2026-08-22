#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 3) in vec4 inWeights;
layout(location = 4) in uvec4 inJoints;

layout(set = 1, binding = 0) uniform AnimUniformBufferObject {
    mat4 mvpMat;
    mat4 mMat;
    mat4 bones[128];
} ubo;

void main() {
    mat4 boneTransform = ubo.bones[int(inJoints.x)] * inWeights.x;
    boneTransform     += ubo.bones[int(inJoints.y)] * inWeights.y;
    boneTransform     += ubo.bones[int(inJoints.z)] * inWeights.z;
    boneTransform     += ubo.bones[int(inJoints.w)] * inWeights.w;

    gl_Position = ubo.mvpMat * boneTransform * vec4(inPosition, 1.0);
}