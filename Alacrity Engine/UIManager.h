#pragma once

#include "imgui-docking/imgui.h"
#include "VulkanRenderer.h"

class UIManager
{

	ImDrawData* draw_data;
	VulkanRenderer* renderer;
	

public:

	UIManager();
	~UIManager();

	bool OnCreate();
	void OnDestroy();
	void Update(const float deltaTime);
	void Render() const;

	ImDrawData* GetDrawData() { return draw_data; }

	Vec4 LightColor[2] { Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0) };
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
};

