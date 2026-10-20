#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=0) out vec3 vNormal;
layout(location=1) out vec3 vWorldPosition;
layout(push_constant) uniform Params {
    mat4 viewProjection;
    vec4 sunDirection;
    vec4 sunColorEnergy;
} p;
void main(){
    vec4 clip=p.viewProjection*vec4(position,1.0);
    clip.y=-clip.y;
    clip.z=(clip.z+clip.w)*0.5;
    gl_Position=clip;
    vNormal=normal;
    vWorldPosition=position;
}
