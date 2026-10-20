#version 450
// Depth-only Sun shadow pass; uses the same world-space vertex stream as
// viewport_direct.vert. Light VP maps depth to Vulkan's [0,1] range.
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(push_constant) uniform SunDepthParams {
    mat4 lightViewProjection;
} sun;
void main() {
    gl_Position = sun.lightViewProjection * vec4(position, 1.0);
}
