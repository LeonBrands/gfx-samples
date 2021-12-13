#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Constants {
    float fade;
} constants;

void main() {
    outColor = vec4(constants.fade * inColor, 1);
}
