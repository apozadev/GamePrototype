#version 450

layout(location = 0) in vec3 aPosition;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

void main() {    
    gl_Position = vec4(aPosition, 1.0);
}
