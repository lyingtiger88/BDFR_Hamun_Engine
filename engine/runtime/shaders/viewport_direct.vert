#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=0) out vec3 vNormal;
layout(push_constant) uniform Params {
    mat4 viewProjection;
    vec4 sunDirection;
    vec4 sunColorEnergy;
} p;
void main() {
    gl_Position=p.viewProjection*vec4(position,1.0);
    vNormal=normal;
}
