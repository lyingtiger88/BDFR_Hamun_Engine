#version 450
layout(location=0) in vec3 vNormal;
layout(location=1) in vec3 vWorld;
layout(location=0) out vec4 outColor;
layout(push_constant) uniform Params {
 vec4 camera; vec4 object; vec4 material; vec4 lightA; vec4 lightB;
} p;
const float PI=3.14159265359;
float hash(vec3 x){return fract(sin(dot(x,vec3(127.1,311.7,74.7)))*43758.5453);}
float noise3(vec3 x){
 vec3 i=floor(x),f=fract(x);f=f*f*(3.-2.*f);
 return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),
 mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1,1,1)),f.x),f.y),f.z);
}
vec3 brdf(vec3 base,float rough,float metal,vec3 N,vec3 V,vec3 L,vec3 lightColor){
 vec3 H=normalize(V+L);
 float ndl=max(dot(N,L),0.),ndv=max(dot(N,V),0.),ndh=max(dot(N,H),0.),vdh=max(dot(V,H),0.);
 float a=max(0.055,rough*rough);float a2=a*a;
 float D=a2/(PI*pow(max(ndh*ndh*(a2-1.)+1.,0.001),2.));
 float k=pow(rough+1.,2.)/8.;
 float G=ndl/(ndl*(1.-k)+k)*ndv/(ndv*(1.-k)+k);
 vec3 f0=mix(vec3(0.04),base,metal);
 vec3 F=f0+(1.-f0)*pow(1.-vdh,5.);
 vec3 spec=D*G*F/max(4.*ndl*ndv,0.002);
 return (base*(1.-metal)*(1.-F)/PI+spec)*lightColor*ndl;
}
// Exact ray/sphere occlusion for the four preview spheres.
// This is a scene-specific hard-shadow test, not a general shadow map.
float sphereShadow(vec3 from,vec3 lightPosition) {
    vec3 ray=lightPosition-from;
    float lengthRay=length(ray);
    vec3 dir=ray/max(lengthRay,0.0001);
    vec3 centers[4]=vec3[4](
        vec3(-3.6,0.0,0.0),vec3(-1.2,0.0,0.0),
        vec3(1.2,0.0,0.0),vec3(3.6,0.0,0.0)
    );
    const float radius=0.87;
    for(int j=0;j<4;++j){
        vec3 offset=from-centers[j];
        float b=dot(offset,dir);
        float c=dot(offset,offset)-radius*radius;
        float discriminant=b*b-c;
        if(discriminant<=0.0)continue;
        float t=-b-sqrt(discriminant);
        if(t>0.002 && t<lengthRay-0.002)return 0.11;
    }
    return 1.0;
}
void main(){
 vec3 N=normalize(vNormal);
 vec3 world=vWorld*p.object.w+p.object.xyz;
 vec3 V=normalize(vec3(0.,0.1,9.)-world);
 float id=p.material.x,t=p.material.y;
 vec3 base;float rough=0.5,metal=0.,transmission=0.;
 if(id<0.5){
    base=vec3(0.42,0.82,0.91);rough=0.065;transmission=0.85;
 }else if(id<1.5){
    float wave=noise3(vWorld*5.5+vec3(t*.4,t*.3,0));
    base=mix(vec3(0.005,0.18,0.43),vec3(0.03,0.62,0.84),wave);
    rough=0.095+wave*0.11;transmission=0.48;
 }else if(id<2.5){
    float grain=noise3(vWorld*28.),mud=noise3(vWorld*5.);
    base=mix(vec3(.115,.052,.02),vec3(.43,.26,.11),mud)*(0.7+0.5*grain);
    rough=0.88;metal=0.;
 }else if(id<3.5){
    float flame=noise3(vec3(vWorld.x*3.8,vWorld.y*6.-t*1.7,vWorld.z*3.8));
    base=mix(vec3(0.98,0.08,0.008),vec3(1.,0.85,0.12),clamp(flame*1.3+vWorld.y*.25,0.,1.));
    rough=0.5;
 }else{
    float dirt=noise3(vWorld*6.0);
    base=mix(vec3(0.13,0.17,0.18),vec3(0.27,0.29,0.28),dirt);
    rough=0.94;
 }
 // Ambient hemisphere plus two independently positioned point lights.
 vec3 col=base*(0.055+max(N.y,0.)*0.08);
 vec3 lights[2]=vec3[2](p.lightA.xyz,p.lightB.xyz);
 vec3 colors[2]=vec3[2](vec3(10.0,8.3,7.0),vec3(3.8,6.8,12.0));
 for(int i=0;i<2;++i){
    vec3 d=lights[i]-world;
    float attenuation=1./(1.+0.07*dot(d,d));
    float shadow=(id>3.5 && id<4.5)?sphereShadow(world+N*0.005,lights[i]):1.0;
    col+=shadow*brdf(base,rough,metal,N,V,normalize(d),colors[i]*attenuation);
 }
 if(transmission>0.){
    // Transparent-looking approximation with Fresnel; no real refraction yet.
    float fresnel=pow(1.-max(dot(N,V),0.),5.);
    col=mix(col,base*(0.17+0.32*max(N.y,0.))+col*0.55,
        transmission*(1.-fresnel)*0.7);
 }
 if(id>2.5 && id<3.5)col+=base*2.2; // Emissive flame demonstration (not particle simulation).
 col=col/(vec3(1.)+col); // simple HDR tone mapping
 col=pow(max(col,vec3(0.)),vec3(1./2.2));
 outColor=vec4(col,1.);
}