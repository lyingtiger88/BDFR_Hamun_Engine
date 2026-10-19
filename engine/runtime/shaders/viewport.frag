#version 450
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform Camera {
    mat4 viewProjection;
    vec4 sunDirection;
    vec4 sunColorEnergy;
    mat4 shadowViewProjection;
} scene;
layout(set=0,binding=1) uniform sampler2D shadowDepth;
float visibility(vec3 n,vec3 direction) {
    vec4 p=scene.shadowViewProjection*vec4(worldPosition,1.0);
    if(p.w<=0.0)return 1.0;
    vec3 shadow=p.xyz/p.w;
    vec2 uv=shadow.xy*vec2(0.5,-0.5)+0.5;
    if(any(lessThan(uv,vec2(0))) || any(greaterThan(uv,vec2(1))) || shadow.z<0.0 || shadow.z>1.0)return 1.0;
    float bias=max(0.0005,0.002*(1.0-max(dot(n,-direction),0.0)));
    vec2 texel=1.0/vec2(textureSize(shadowDepth,0));
    float result=0.0;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)
        result+=shadow.z-bias<=texture(shadowDepth,uv+vec2(x,y)*texel).r?1.0:0.0;
    return result/9.0;
}
void main(){
    vec3 n=normalize(worldNormal);
    vec3 direction=normalize(scene.sunDirection.xyz);
    float cosine=max(dot(n,-direction),0.0);
    float shadow=cosine>0.0?visibility(n,direction):0.0;
    vec3 linear=vec3(0.12)+scene.sunColorEnergy.rgb*scene.sunColorEnergy.a*cosine*shadow;
    vec3 mapped=linear/(vec3(1.0)+linear);
    fragColor=vec4(pow(mapped,vec3(1.0/2.2)),1.0);
}
