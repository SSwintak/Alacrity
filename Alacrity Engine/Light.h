#pragma once

#include "VMath.h"

using namespace MATH;

class Light
{
	// setup light so it supports arrays
private:

	Vec3 pos;



public:

	inline Vec3 GetLightPos() { return pos; }

	inline void SetLightPos(Vec3 pos_) { pos = pos_; }


};

