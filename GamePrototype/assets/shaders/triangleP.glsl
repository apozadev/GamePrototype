#version 450
#pragma shader_stage(fragment)

layout(set = 0, binding = 0) uniform MaterialConstants
{
    vec4 vColor;
};

layout(location = 0) out vec4 oColor;

void main() {
    oColor = vColor;
}
