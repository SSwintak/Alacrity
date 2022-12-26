#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(set = 1, binding = 0) uniform sampler2D texSampler;


layout(location = 0) in  vec3 vertNormal;
layout(location = 1) in  vec3 lightDir[2];
layout(location = 3) in  vec4 lightColor[2];
layout(location = 5) in  vec3 eyeDir;
layout(location = 6) in  vec2 fragTexCoord; 
 

layout(location = 0) out vec4 fragColor;

void main() { 
	//const vec4 ks = vec4(0.6, 0.6, 0.6, 0.0);
	//const vec4 kd = vec4(0.6, 0.6, 0.6, 0.0); /// const means it cannot be changed just like C++
	//const vec4 ka = 0.1 * kd;
	//
	//float diff = max(dot(vertNormal, lightDir), 0.0);
	///// Reflection is based incedent which means a vector from the light source
	///// not the direction to the light source
	//vec3 reflection = normalize(reflect(-lightDir, vertNormal));
	//float spec = max(dot(eyeDir, reflection), 0.0);
	//if(diff > 0.0){
	//	spec = pow(spec,14.0);
	//}
	//fragColor =  ka + (diff * kd) + (spec * ks);	


	vec4 kt;
	vec4 ka;
	vec4 color;
	
	
	kt = texture(texSampler, fragTexCoord);
	ka = 0.1 * kt;
	
	color = ka;
	
	const vec4 kd[2] = vec4[2](lightColor[0], lightColor[1]);// light color
	vec4 ks[2];
	float diff[2];
	vec3 reflection[2];
	float spec[2];	 

	for(int i = 0; i < 2; i++)
	{
		ks[i] = 0.5 * kd[i];

		diff[i] = max(dot(vertNormal, lightDir[i]), 0.0);
		reflection[i] = normalize(reflect(-lightDir[i], vertNormal));

		spec[i] = max(dot(eyeDir, reflection[i]), 0.0);
		spec[i] = pow(spec[i], 14.0);

		color += (diff[i] * kt * kd[i]) + (spec[i] * ks[i]);
	}

	
	fragColor = color;


} 

