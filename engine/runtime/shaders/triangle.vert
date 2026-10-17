#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=0) out vec3 vNormal;
layout(location=1) out vec3 vWorld;
layout(push_constant) uniform Params {
 vec4 camera; vec4 object; vec4 material;
} p;
void main(){
 vec3 pos=position*p.object.w+p.object.xyz;
 float cy=cos(p.camera.x),sy=sin(p.camera.x),cp=cos(p.camera.y),sp=sin(p.camera.y);
 vec3 rotated=vec3(cy*pos.x+sy*pos.z,pos.y,-sy*pos.x+cy*pos.z);
 rotated=vec3(rotated.x,cp*rotated.y-sp*rotated.z,sp*rotated.y+cp*rotated.z);
 rotated.z+=p.camera.z;
 float n=0.1,f=100.0;
 gl_Position=vec4(rotated.x*1.7/p.camera.w,-rotated.y*1.7,(f/(f-n))*rotated.z-f*n/(f-n),rotated.z);
 vNormal=normal;
 vWorld=position;
}