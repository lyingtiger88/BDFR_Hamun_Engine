#version 450
// Vulkan Sun shadow-map depth pass. Positions are already in world space.
layout(location=0) in vec3 position;
layout(push_constant) uniform SunDepthParams {
    mat4 lightViewProjection;
} sun;
void main() {
    vec4 clip = sun.lightViewProjection * vec4(position, 1.0);
    // Shadow matrices are constructed with a Vulkan [0, 1] depth range.
    gl_Position = clip;
}
