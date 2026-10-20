#version 450
layout(location=0) in vec3 vNormal;
layout(location=1) in vec3 vWorldPosition;
layout(location=0) out vec4 outColor;
layout(push_constant) uniform Params {
    mat4 viewProjection;
    vec4 sunDirection;
    vec4 sunColorEnergy;
} p;
layout(set=0,binding=0) uniform sampler2DShadow shadowDepth;
layout(set=0,binding=1,std140) uniform SunShadow {
    mat4 lightViewProjection;
    vec4 settings;
} s;
void main(){
    vec3 n=normalize(vNormal);
    vec3 lightDir=normalize(-p.sunDirection.xyz);
    float diffuse=max(dot(n,lightDir),0.0);
    float visibility=1.0;
    if(s.settings.x>0.5 && diffuse>0.0){
        vec4 c=s.lightViewProjection*vec4(vWorldPosition,1.0);
        if(c.w>0.0){
            vec3 q=c.xyz/c.w;
            vec2 uv=vec2(q.x*.5+.5,1.0-(q.y*.5+.5));
            if(all(greaterThanEqual(uv,vec2(0.0))) &&
               all(lessThanEqual(uv,vec2(1.0))) && q.z>=0.0 && q.z<=1.0){
                vec2 texel=1.0/vec2(textureSize(shadowDepth,0));
                visibility=0.0;
                for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
                    visibility+=texture(shadowDepth,vec3(uv+vec2(x,y)*texel*s.settings.z,q.z-s.settings.y));
                }
                visibility/=9.0;
            }
        }
    }
    vec3 linear=vec3(0.10,0.12,0.15)+p.sunColorEnergy.rgb*
                (p.sunColorEnergy.a*diffuse*visibility);
    vec3 mapped=linear/(vec3(1.0)+linear);
    outColor=vec4(pow(mapped,vec3(1.0/2.2)),1.0);
}
