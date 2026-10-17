#version 450
layout(location=0) in vec3 vNormal;
layout(location=1) in vec3 vWorld;
layout(location=0) out vec4 outColor;
layout(push_constant) uniform Params {
 vec4 camera; vec4 object; vec4 material;
} p;
float hash(vec3 x){return fract(sin(dot(x,vec3(127.1,311.7,74.7)))*43758.5453);}
float noise3(vec3 x){
 vec3 i=floor(x),f=fract(x);f=f*f*(3.0-2.0*f);
 float a=hash(i+vec3(0,0,0)),b=hash(i+vec3(1,0,0));
 float c=hash(i+vec3(0,1,0)),d=hash(i+vec3(1,1,0));
 float e=hash(i+vec3(0,0,1)),g=hash(i+vec3(1,0,1));
 float h=hash(i+vec3(0,1,1)),j=hash(i+vec3(1,1,1));
 return mix(mix(mix(a,b,f.x),mix(c,d,f.x),f.y),mix(mix(e,g,f.x),mix(h,j,f.x),f.y),f.z);
}
void main(){
 vec3 N=normalize(vNormal);
 vec3 V=normalize(vec3(0.2,0.2,1.0));
 vec3 L=normalize(vec3(-0.55,0.9,1.1));
 float diffuse=max(dot(N,L),0.0);
 float rim=pow(1.0-max(dot(N,V),0.0),3.0);
 float spec=pow(max(dot(reflect(-L,N),V),0.0),55.0);
 float id=p.material.x;
 float t=p.material.y;
 vec3 col;
 if(id<0.5){ // stylized glass preview, not physical refraction
    col=mix(vec3(0.06,0.23,0.34),vec3(0.48,0.9,0.96),rim);
    col+=vec3(1.0)*spec*1.7;
    col+=0.18*pow(max(dot(N,V),0.0),2.0);
 }else if(id<1.5){ // water surface-like sphere
    float wave=noise3(vWorld*5.5+vec3(t*0.65,t*0.3,0));
    col=mix(vec3(0.008,0.10,0.35),vec3(0.05,0.8,0.92),0.25+0.65*wave);
    col*=0.35+diffuse*0.65;
    col+=rim*vec3(0.15,0.85,1.0)+spec*vec3(1.0);
 }else if(id<2.5){ // soil
    float grain=noise3(vWorld*22.0);
    float mud=noise3(vWorld*5.0);
    col=mix(vec3(0.13,0.065,0.026),vec3(0.45,0.29,0.12),mud);
    col*=0.75+0.50*grain;
    col*=0.27+0.73*diffuse;
 }else{ // animated stylized fire, emissive preview
    float flame=noise3(vec3(vWorld.x*4.0,vWorld.y*7.0-t*2.2,vWorld.z*4.0));
    float strength=clamp(0.5+0.6*flame+0.25*vWorld.y,0.0,1.0);
    col=mix(vec3(0.95,0.11,0.009),vec3(1.0,0.9,0.2),strength);
    col+=vec3(0.35,0.06,0.0)*rim;
 }
 outColor=vec4(col,1.0);
}