#include "PrecompiledHeader.h"
#include "UIManager.h"

#include "imgui-docking/backends/imgui_impl_sdl.h"
#include "imgui-docking/backends/imgui_impl_vulkan.h"



UIManager::UIManager()
{
    /*renderer = new VulkanRenderer();*/
}

UIManager::~UIManager()
{
    /*ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (renderer) delete renderer;*/
}

bool UIManager::OnCreate()
{
	return true;
}

void UIManager::OnDestroy()
{
}

void UIManager::Update(const float deltaTime)
{
}

void UIManager::Render() const
{
    //// these probably shouldn't be static
    //static bool Show_Demo_Window = true;
    //static bool show_another_window = false;
    //static bool show_normals = false;

    //ImGui_ImplVulkan_NewFrame();
    //ImGui_ImplSDL2_NewFrame();
    //ImGui::NewFrame();

    //// for initial testing
    //if (Show_Demo_Window)
    //{
    //    ImGui::ShowDemoWindow(&Show_Demo_Window);
    //    //ImGui::SetWindowSize(ImVec2(100, 100));
    //}
    //static float f = 0.0f;
    //static int counter = 0;

    //// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    //{
    //    static float f = 0.0f;
    //    static int counter = 0;

    //    ImGui::Begin("Main Window");                              // Create a window called "Hello, world!" and append into it.

    //    ImGui::Text("This is some useful text.");                 // Display some text (you can use a format strings too)
    //    ImGui::Checkbox("Demo Window", &Show_Demo_Window);        // Edit bools storing our window open/close state
    //    ImGui::Checkbox("Another Window", &show_another_window);
    //    ImGui::Checkbox("Normals On/Off", &show_normals);

    //    ImGui::SliderFloat("Model Position", &f, 0.0f, 1.0f);     // Edit 1 float using a slider from 0.0f to 1.0f
    //    ImGui::ColorEdit3("clear color", (float*)(&clear_color)); // Edit 3 floats representing a color
    //    ImGui::ColorEdit3("light1 color", (float*)(&LightColor[0]));
    //    ImGui::ColorEdit3("light2 color", (float*)(&LightColor[1]));

    //    if (ImGui::Button("Button"))                              // Buttons return true when clicked (most widgets return true when edited/activated)
    //        counter++;
    //    ImGui::SameLine();
    //    ImGui::Text("counter = %d", counter);

    //    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    //    ImGui::End();
    //}

    //// 3. Show another simple window.
    //if (show_another_window)
    //{
    //    ImGui::Begin("Another Window", &show_another_window);     // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
    //    ImGui::Text("Hello from another window!");
    //    if (ImGui::Button("Close Me"))
    //        show_another_window = false;
    //    ImGui::End();
    //}

    //ImGui::Render();


    //if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    //{
    //    ImGui::UpdatePlatformWindows();
    //    ImGui::RenderPlatformWindowsDefault();
    //}
    //draw_data = ImGui::GetDrawData();

}

