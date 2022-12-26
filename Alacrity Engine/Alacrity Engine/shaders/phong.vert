#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(set = 0, binding = 1) uniform UniformLightBufferObject {
    vec4 lightPos[2];
	vec4 lightColor[2];
	vec4 normalColor[2];
} LT_ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 vertNormal;
layout(location = 1) out vec3 lightDir[2];//arrays take more layout locations
layout(location = 3) out vec4 lightColor[2];
layout(location = 5) out vec3 eyeDir; 
layout(location = 6) out vec2 fragTexCoord;


layout(std140, push_constant) uniform constants{
	mat4 render_mat4;
	mat4 normal_mat4;
} PushConstants;


void main() {
	//vertNormal = normalize(normalMatrix * vNormal.xyz); /// Rotate the normal to the correct orientation 
	//vec3 vertPos = vec3(viewMatrix * modelMatrix * vVertex); /// This is the position of the vertex from the origin
	//vec3 vertDir = normalize(vertPos);
	//eyeDir = -vertDir;
	//lightDir = normalize(lightPos - vertPos); /// Create the light direction.
	//
	//gl_Position =  projectionMatrix * viewMatrix * modelMatrix * vVertex;
	
	fragTexCoord = inTexCoord;
	vertNormal = normalize(mat3(PushConstants.normal_mat4) * inNormal);
	vec3 vertPos = vec3(ubo.view * PushConstants.render_mat4 * vec4(inPosition, 1.0));
	vec3 vertDir = normalize(vertPos);
	eyeDir = -vertDir;
	for(int i = 0; i < 2; i++)
	{
		lightDir[i] = normalize(vec3(LT_ubo.lightPos[i]) - vertPos);
		lightColor[i] = LT_ubo.lightColor[i];
	}

	gl_Position = ubo.proj * ubo.view * PushConstants.render_mat4 * vec4(inPosition, 1.0);

}
