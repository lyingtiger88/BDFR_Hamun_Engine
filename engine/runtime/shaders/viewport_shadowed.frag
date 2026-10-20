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
    mat4 pointFaces[6];
    vec4 pointPosition;
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
    // One shadow map: selected Sun, Spot, Area, or scene-facing Point.
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
                float edge=cos(clamp(l.settings.y,0.05,3.10)*0.5);
                attenuation*=smoothstep(edge,min(1.0,edge+0.10),cone);
            }
            // Area preview uses its center as a point emitter until area
            // dimensions and sampled soft shadows are implemented.
        }
        float diffuse=max(dot(n,lightDir),0.0);
        float visibility=1.0;
        if(int(s.settings.w+0.5)==i+1 && s.settings.x>0.5 && diffuse>0.0){
            int face=-1;
            mat4 shadowMatrix=s.lightViewProjection;
            if(kind==1 && s.pointPosition.w>0.5){
                // Point lights are omnidirectional: select one of six fixed
                // world-axis faces from the fragment's position, never from
                // the Blender light object's rotation.
                vec3 ray=vWorldPosition-s.pointPosition.xyz;
                vec3 a=abs(ray);
                if(a.x>=a.y && a.x>=a.z)face=ray.x>=0.0?0:1;
                else if(a.y>=a.z)face=ray.y>=0.0?2:3;
                else face=ray.z>=0.0?4:5;
                shadowMatrix=s.pointFaces[face];
            }
            vec4 c=shadowMatrix*vec4(vWorldPosition,1.0);
            if(c.w>0.0){
                vec3 q=c.xyz/c.w;
                // Positive-height Vulkan viewports map NDC +Y toward increasing framebuffer Y.
                // Sample shadow depth in the same orientation used to rasterize it.
                vec2 uv=vec2(q.x*.5+.5,q.y*.5+.5);
                if(all(greaterThanEqual(uv,vec2(0.0))) &&
                   all(lessThanEqual(uv,vec2(1.0))) && q.z>=0.0 && q.z<=1.0){
                    vec2 texel=1.0/vec2(textureSize(shadowDepth,0));
                    vec2 offset=vec2(0.0),scale=vec2(1.0);
                    if(face>=0){
                        offset=vec2(float(face%3)/3.0,float(face/3)/2.0);
                        scale=vec2(1.0/3.0,1.0/2.0);
                    }
                    // Do not let PCF taps cross atlas tile boundaries.
                    vec2 halfPixel=texel*0.5;
                    visibility=0.0;
                    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
                        vec2 local=uv+vec2(x,y)*(texel/scale)*s.settings.z;
                        vec2 atlasUV=clamp(offset+local*scale,
                                           offset+halfPixel,
                                           offset+scale-halfPixel);
                        visibility+=texture(shadowDepth,vec3(atlasUV,q.z-s.settings.y));
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
