#version 450
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
layout(set=0,binding=0,std140) uniform Camera {
    mat4 viewProjection;
    vec4 sunDirection;
    vec4 sunColorEnergy;
    mat4 shadowViewProjection;
} scene;
void main() {
    worldPosition=inPosition;
    worldNormal=normalize(inNormal);
    gl_Position=scene.viewProjection*vec4(inPosition,1.0);
}
