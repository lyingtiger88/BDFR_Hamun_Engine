#version 450
layout(push_constant) uniform Camera {
  vec4 settings; // yaw, pitch, distance, aspect ratio
} camera;
layout(location=0) out vec3 outColor;
const vec3 corners[8] = vec3[8](
 vec3(-1,-1,-1),vec3(1,-1,-1),vec3(1,1,-1),vec3(-1,1,-1),
 vec3(-1,-1,1),vec3(1,-1,1),vec3(1,1,1),vec3(-1,1,1));
const int indices[36] = int[36](
 4,5,6, 4,6,7, 1,0,3, 1,3,2,
 0,4,7, 0,7,3, 5,1,2, 5,2,6,
 3,7,6, 3,6,2, 0,1,5, 0,5,4);
void main(){
 vec3 p=corners[indices[gl_VertexIndex]];
 float cy=cos(camera.settings.x), sy=sin(camera.settings.x);
 float cp=cos(camera.settings.y), sp=sin(camera.settings.y);
 p=vec3(cy*p.x+sy*p.z,p.y,-sy*p.x+cy*p.z);
 p=vec3(p.x,cp*p.y-sp*p.z,sp*p.y+cp*p.z);
 p.z += camera.settings.z;
 // Vulkan depth range 0..1: near=0.1, far=100.
 float n=0.1, f=100.0;
 gl_Position=vec4(p.x*1.7/camera.settings.w,-p.y*1.7,(f/(f-n))*p.z-(f*n/(f-n)),p.z);
 const vec3 faceColors[6]=vec3[6](
 vec3(0.2,0.8,0.9),vec3(0.95,0.3,0.35),vec3(0.4,0.95,0.45),
 vec3(1.0,0.72,0.25),vec3(0.55,0.45,1.0),vec3(0.85,0.85,0.9));
 outColor=faceColors[gl_VertexIndex/6]*(0.65+0.35*(p.y+1.5)/3.0);
}
