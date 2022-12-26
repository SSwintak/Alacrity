#pragma once

#include "VMath.h"
#include "MMath.h"
#include <vector>

using namespace MATH;

class MovableCamera
{
public:

	MovableCamera(float verticalFOV, float near, float far);

	void OnUpdate(float ts);
	void OnResize(uint32_t width, uint32_t height);

	const Matrix4& GetProjection() const { return projection; }
	const Matrix4& GetInverseProjection() const { return inverseProjection; }
	const Matrix4& GetView() const { return view; }
	const Matrix4& GetInverseView() const { return inverseView; }

	const Vec3& GetPosition() const { return position; }
	const Vec3& GetDirection() const { return forwardDirection; }

	const std::vector<Vec3>& GetRayDirections() const { return rayDirections; }

	float GetRotationSpeed();
private:
	void RecalculateProjection();
	void RecalculateView();
	void RecalculateRayDirections();
private:
	Matrix4 projection;
	Matrix4 view;
	Matrix4 inverseProjection;
	Matrix4 inverseView;

	float verticalFOV = 45.0f;
	float nearClip = 0.1f;
	float farClip = 100.0f;

	Vec3 position{ 0.0f, 0.0f, 0.0f };
	Vec3 forwardDirection{ 0.0f, 0.0f, 0.0f };

	// Cached ray directions
	std::vector<Vec3> rayDirections;

	Vec2 lastMousePosition{ 0.0f, 0.0f };

	uint32_t viewportWidth = 0, viewportHeight = 0;

};

