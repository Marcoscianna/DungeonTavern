#version 450
layout(set = 1, binding = 0) uniform UniformBufferObject {
	mat4 mvpMat;
	mat4 mMat;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNorm;

void main() {
	gl_Position = ubo.mvpMat * vec4(inPosition, 1.0);
	fragPos = (ubo.mMat * vec4(inPosition, 1.0)).xyz;
	fragUV  = inUV;
    fragNorm = mat3(transpose(inverse(ubo.mMat))) * inNormal;
}