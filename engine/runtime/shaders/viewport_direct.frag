#version 450
layout(location=0) in vec3 vNormal;
layout(location=0) out vec4 outColor;
layout(push_constant) uniform Params {
    mat4 viewProjection;
    vec4 sunDirection;
    vec4 sunColorEnergy;
} p;
void main() {
    vec3 n=normalize(vNormal);
    vec3 lightDir=normalize(-p.sunDirection.xyz);
    float diffuse=max(dot(n,lightDir),0.0);
    vec3 linear=vec3(0.10,0.12,0.15)+p.sunColorEnergy.rgb*(p.sunColorEnergy.a*diffuse);
    vec3 mapped=linear/(vec3(1.0)+linear);
    outColor=vec4(pow(mapped,vec3(1.0/2.2)),1.0);
}
