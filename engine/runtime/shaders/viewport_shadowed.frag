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
struct SceneLight {
    vec4 positionType;
    vec4 directionRange;
    vec4 colorEnergy;
    vec4 settings;
};
layout(set=0,binding=2,std140) uniform SceneLights {
    SceneLight lights[32];
    int count;
} scene;
void main(){
    vec3 n=normalize(vNormal);
    vec3 linear=vec3(0.10,0.12,0.15);
    // The current single shadow map belongs to the first enabled Sun.
    bool sunShadowUsed=false;
    for(int i=0;i<min(scene.count,32);++i){
        SceneLight l=scene.lights[i];
        int kind=int(l.positionType.w+0.5);
        vec3 lightDir=vec3(0.0);
        float attenuation=1.0;
        if(kind==0){
            lightDir=normalize(-l.directionRange.xyz);
        }else{
            vec3 delta=l.positionType.xyz-vWorldPosition;
            float d=max(length(delta),0.001);
            lightDir=delta/d;
            // Smooth inverse-square approximation; distance is in Blender world units.
            attenuation=1.0/(1.0+0.1*d*d);
            if(kind==2){
                // Initial Spot preview cone; actual Blender spot_size/spot_blend
                // must be sent through an extended light ABI.
                vec3 axis=normalize(l.directionRange.xyz);
                float cone=dot(-lightDir,axis);
                attenuation*=smoothstep(0.65,0.82,cone);
            }
            // Area preview uses its center as a point emitter until area
            // dimensions and sampled soft shadows are implemented.
        }
        float diffuse=max(dot(n,lightDir),0.0);
        float visibility=1.0;
        if(kind==0 && !sunShadowUsed && s.settings.x>0.5 && diffuse>0.0){
            sunShadowUsed=true;
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
        float energy=0.8*log(1.0+l.colorEnergy.w);
        linear+=l.colorEnergy.rgb*(energy*diffuse*attenuation*visibility);
    }
    vec3 mapped=linear/(vec3(1.0)+linear);
    outColor=vec4(pow(mapped,vec3(1.0/2.2)),1.0);
}
