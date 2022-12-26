#include "PrecompiledHeader.h"
#include "MovableCamera.h"

MovableCamera::MovableCamera(float verticalFOV, float near, float far):
	verticalFOV(verticalFOV), nearClip(near), farClip(far)
{
	forwardDirection = Vec3(0.0f, 0.0f, -1.0f);
	position = Vec3(0.0f, 0.0f, 3.0f);
}

void MovableCamera::OnUpdate(float ts)
{

}
