#version 450
layout(location=0) in vec3 vNormal;
layout(location=1) in vec3 vWorldPosition;
layout(location=2) in vec2 vUV;
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
            if(kind==1 && s.pointPosition.w>1.5){
                // Diagnostic view: R/G encode the chosen face's projected UV,
                // B encodes normalized depth. The face identity is visible as
                // six different depth-projection orientations on the receiver.
                vec3 qDebug=c.xyz/max(abs(c.w),0.00001);
                vec2 uvDebug=qDebug.xy*0.5+0.5;
                outColor=vec4(clamp(vec3(uvDebug,qDebug.z),0.0,1.0),1.0);
                return;
            }
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
                    // Rotated Poisson PCF removes the visible 3x3 grid pattern.
                    // Keep samples within the selected Point atlas tile.
                    const vec2 taps[12]=vec2[12](
                        vec2(-0.326,-0.406),vec2(-0.840,-0.074),
                        vec2(-0.696, 0.457),vec2(-0.203, 0.621),
                        vec2( 0.962,-0.195),vec2( 0.473,-0.480),
                        vec2( 0.519, 0.767),vec2( 0.185,-0.893),
                        vec2( 0.507, 0.064),vec2( 0.896, 0.412),
                        vec2(-0.322,-0.933),vec2(-0.792,-0.598)
                    );
                    vec2 halfPixel=texel*0.5;
                    float angle=6.2831853*fract(sin(dot(gl_FragCoord.xy,
                                      vec2(12.9898,78.233)))*43758.5453);
                    mat2 rotation=mat2(cos(angle),-sin(angle),
                                       sin(angle),cos(angle));
                    // EEVEE-inspired distance-aware filter footprint. Point's
                    // radius and Area's half-width come from Blender light data.
                    // This is a bounded PCF approximation, not full PCSS.
                    float sourceRadius=max(0.0,l.settings.z);
                    float receiverDistance=max(length(vWorldPosition-l.positionType.xyz),0.05);
                    float apparentSize=sourceRadius/receiverDistance;
                    // Screen-space softness responds to emitter size and
                    // receiver distance, without modifying shadow projection.
                    float sizeSoftness=(kind==1 || kind==3)
                        ?clamp(apparentSize*12.0,0.0,8.0):0.0;
                    float radius=clamp(max(0.0,s.settings.z)*1.5+sizeSoftness,0.0,12.0);
                    visibility=0.0;
                    for(int tap=0;tap<12;++tap){
                        vec2 local=uv+(rotation*taps[tap])*(texel/scale)*radius;
                        vec2 atlasUV=clamp(offset+local*scale,
                                           offset+halfPixel,
                                           offset+scale-halfPixel);
                        visibility+=texture(shadowDepth,vec3(atlasUV,q.z-s.settings.y));
                    }
                    visibility/=12.0;
                }
            }
        }
        float energy=0.8*log(1.0+l.colorEnergy.w);
        linear+=l.colorEnergy.rgb*(energy*diffuse*attenuation*visibility);
    }
    vec3 mapped=linear/(vec3(1.0)+linear);
    outColor=vec4(pow(mapped,vec3(1.0/2.2)),1.0);
}
