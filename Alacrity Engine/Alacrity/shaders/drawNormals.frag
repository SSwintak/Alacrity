#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform UniformLightBufferObject {
    vec4 lightPos[2];
	vec4 lightColor[2];
	vec4 normalColor[2];
} LT_ubo;



layout(location = 0) out vec4 fragColor;

void main() {

    
    //vec4(0.0, 1.0, 1.0, 0.0)
    fragColor = LT_ubo.normalColor[0];
}