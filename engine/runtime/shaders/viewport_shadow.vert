#version 450
layout(location=0) in vec3 inPosition;
layout(set=0,binding=0,std140) uniform Camera {
    mat4 viewProjection;
    vec4 sunDirection;
    vec4 sunColorEnergy;
    mat4 shadowViewProjection;
} scene;
void main(){gl_Position=scene.shadowViewProjection*vec4(inPosition,1.0);}
