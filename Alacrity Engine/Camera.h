#pragma once


//#include "Matrix.h"
//#include "MMath.h"
#include "PrecompiledHeader.h"


using namespace MATH;
class Camera {
private:
	Matrix4 projection;
	Matrix4 view;
	/*glm::mat4 projection;
	glm::mat4 view;*/
	Matrix4 rotation;
	Matrix4 translate;
	/*glm::mat4 rotation;
	glm::mat4 translate;*/

public:
	Camera();
	~Camera();
	void Perspective(const float fovy_, const float aspectRatio_, const float near_, const float far_);
	void LookAt(const Vec3& eye, const Vec3& at, const Vec3& up);
	//void LookAt(const glm::vec3& eye, const glm::vec3& at, const glm::vec3& up);
	inline Matrix4 GetProjectionMatrix() { return projection; }
	inline Matrix4 GetViewMatrix() { return view; }
	//inline glm::mat4 GetProjectionMatrix() { return projection; }
	//inline glm::mat4 GetViewMatrix() { return view; }

	inline Matrix4 GetRotMatrix() { return rotation; }
	inline Matrix4 GetTransMatrix() { return translate; }
	/*inline glm::mat4 GetRotMatrix() { return rotation; }
	inline glm::mat4 GetTransMatrix() { return translate; }*/

	inline void SetViewMatrix(Vec3 translate_, float angle_, Vec3 rotation_) 
	{
		translate = MMath::translate(translate_);
		rotation = MMath::rotate(angle_, rotation_);
		view = translate * rotation;
	}

	/*inline void SetViewMatrix(glm::vec3 translate_, float angle_, glm::vec3 rotation_)
	{
		translate = glm::translate(translate, translate_);
		rotation = glm::rotate(rotation, angle_, rotation_);
		view = translate * rotation;
	}*/

};

