#version 450
layout(location=0) out vec3 outColor;
void main() {
    vec2 p[3] = vec2[](vec2(0.0,-0.7),vec2(0.7,0.65),vec2(-0.7,0.65));
    vec3 c[3] = vec3[](vec3(1,0.18,0.25),vec3(0.18,1,0.35),vec3(0.2,0.4,1));
    gl_Position = vec4(p[gl_VertexIndex],0,1);
    outColor = c[gl_VertexIndex];
}
