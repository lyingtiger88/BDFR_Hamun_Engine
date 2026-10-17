#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=0) out vec3 outColor;
layout(push_constant) uniform Params {
 vec4 camera; // yaw,pitch,distance,aspect
 vec4 object; // x,y,z,scale
} params;
void main(){
 vec3 p=position*params.object.w+params.object.xyz;
 float cy=cos(params.camera.x),sy=sin(params.camera.x);
 float cp=cos(params.camera.y),sp=sin(params.camera.y);
 p=vec3(cy*p.x+sy*p.z,p.y,-sy*p.x+cy*p.z);
 p=vec3(p.x,cp*p.y-sp*p.z,sp*p.y+cp*p.z);
 p.z+=params.camera.z;
 float n=0.1,f=100.0;
 gl_Position=vec4(p.x*1.7/params.camera.w,-p.y*1.7,(f/(f-n))*p.z-f*n/(f-n),p.z);
 outColor=color;
}