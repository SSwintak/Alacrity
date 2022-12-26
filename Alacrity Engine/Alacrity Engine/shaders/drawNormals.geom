#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (triangles) in; /// bringing in triangles

layout(location = 8) in vec4 normalColor[];

layout (line_strip, max_vertices = 2) out; 

layout (std140, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
};

layout (location = 0) in VertexStage {
    vec3 normal;
} vs_in[];

layout(location = 9) out vec4 NormalColor;


void main() {
    float length  = 0.1;
    for(int index = 0; index < 3; index++){
        gl_Position = proj * gl_in[index].gl_Position;
        NormalColor = normalColor[index];
        EmitVertex();

        gl_Position = proj * (gl_in[index].gl_Position + (vec4(vs_in[index].normal, 1.0) * length));
        EmitVertex();

        EndPrimitive();
    }
}