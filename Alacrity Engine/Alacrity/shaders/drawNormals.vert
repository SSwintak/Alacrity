#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inNormal;


layout (std140, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
};



layout(push_constant) uniform constants {
	mat4 render_mat4;
	mat4 normal_mat4;
} push;


layout (location = 0) out VertexStage {
    vec3 normal;
} vs_out;

layout(location = 8) out vec4 normalColor[2];

void main() {
    
    //normalColor[0] = LT_ubo.normalColor[0];
    //normalColor[1] = LT_ubo.normalColor[1];
    gl_Position = view * push.render_mat4 * inPosition;
    vs_out.normal = mat3(push.normal_mat4) * inNormal;
}