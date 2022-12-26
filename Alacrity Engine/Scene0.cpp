#include "PrecompiledHeader.h"
//#include <glew.h>
//#include <iostream>
#include "Scene0.h"
//#include "MMath.h"
//#include "Debug.h"
#include "VulkanRenderer.h"
#include "OpenGLRenderer.h"
#include "Camera.h"


Scene0::Scene0(Renderer *renderer_): 
	Scene(nullptr),renderer(renderer_), camera(nullptr) {
	camera = new Camera();
	UI = new UIManager();
	controller = new Controller(0);
	
	Debug::Info("Created Scene0: ", __FILE__, __LINE__);
}

Scene0::~Scene0() {
	if (camera) delete camera;
	if (UI) delete UI;
	if (controller) delete controller;
	
	if(AudioEngine) AudioEngine->drop();// delete the engine

}

bool Scene0::OnCreate() {
	int width = 0, height = 0;
	float aspectRatio;
	std::thread th_sound;
	switch (renderer->getRendererType()){
	case RendererType::VULKAN:
		
		SDL_GetWindowSize(dynamic_cast<VulkanRenderer*>(renderer)->GetWindow(), &width, &height);
		aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		camera->Perspective(45.0f, aspectRatio, 0.5f, 20.0f);
		//camera->LookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
		
		camera->SetViewMatrix(Vec3(0.0f, -2.5f, -10.0f), 0.0f, Vec3(0.0f, 0.0f, 1.0f));
		//camera->SetViewMatrix(glm::vec3(0.0f, -2.5f, -10.0f), 0.0f, glm::vec3(0.0f, 0.0f, 1.0f));

		//movableCamera = new MovableCamera(45.0f, 0.5f, 20.0f);

		//models.emplace("Batman", Matrix4());
		//models.emplace("Skull", Matrix4());

		AudioEngine = irrklang::createIrrKlangDevice();// create the engine
		if (!AudioEngine)
		{
			std::cout << "Failed to Start IrrKlang!\n";
			return false;// failed to start the engine
		}

		// SOUND !!!!!!!!!!!!																	// SOUND !!!!!!!!!!!!!
		//irrklang::ISound* music = AudioEngine->play2D("media/BatmanTheme.wav", false, false, true);// (TODO): multithread
		th_sound = std::thread(&Scene0::soundFunc, this);
		th_sound.join();
		//music->setVolume(0.25f);

		break;

	case RendererType::OPENGL:

		SDL_GetWindowSize(dynamic_cast<OpenGLRenderer*>(renderer)->GetWindow(), &width, &height);
		aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		camera->Perspective(45.0f, aspectRatio, 0.5f, 20.0f);
		//camera->LookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));

		camera->SetViewMatrix(Vec3(0.0f, -2.5f, -10.0f), 0.0f, Vec3(0.0f, 0.0f, 1.0f));

		break;
	}
		
	

	return true;
}

void Scene0::HandleEvents(const SDL_Event& sdlEvent) {
	if (sdlEvent.type == SDL_WINDOWEVENT) {
		switch (sdlEvent.window.event) {
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			printf("Window ID %d\n", sdlEvent.window.windowID);
			printf("size changed %d %d\n", sdlEvent.window.data1, sdlEvent.window.data2);
			if (sdlEvent.window.windowID == 1)
			{
				//dynamic_cast<VulkanRenderer*>(renderer)->SetResize(true);
				float aspectRatio = static_cast<float>(sdlEvent.window.data1) / static_cast<float>(sdlEvent.window.data2);
				camera->Perspective(45.0f, aspectRatio, 0.5f, 20.0f);
			}
			break;			
		}
	}
	else if (sdlEvent.type == SDL_MOUSEBUTTONDOWN)
	{
		switch (sdlEvent.button.button)
		{
		case SDL_BUTTON_RIGHT:
			break;
		}
	}
	std::thread th_ctrlr(&Scene0::GetControllerEvents, this);
	th_ctrlr.detach();
}

void Scene0::GetControllerEvents()
{		
	//std::cout << controller->IsConnected() << '\n';
	while (controller->IsConnected())
	{

		XINPUT_STATE xinput_state = controller->GetControllerState();

		//controller buttons
		if (xinput_state.Gamepad.wButtons > 0)
		{
			switch (xinput_state.Gamepad.wButtons)
			{
			case XINPUT_GAMEPAD_A:
				std::cout << "XINPUT_GAMEPAD_A\n";
				break;

			case XINPUT_GAMEPAD_B:
				std::cout << "XINPUT_GAMEPAD_B\n";
				break;

			case XINPUT_GAMEPAD_X:
				std::cout << "XINPUT_GAMEPAD_X\n";
				break;

			case XINPUT_GAMEPAD_Y:
				std::cout << "XINPUT_GAMEPAD_Y\n";
				break;

			case XINPUT_GAMEPAD_DPAD_DOWN:
				std::cout << "XINPUT_GAMEPAD_DPAD_DOWN\n";
				break;

			case XINPUT_GAMEPAD_DPAD_LEFT:
				std::cout << "XINPUT_GAMEPAD_DPAD_LEFT\n";
				break;

			case XINPUT_GAMEPAD_DPAD_RIGHT:
				std::cout << "XINPUT_GAMEPAD_DPAD_RIGHT\n";
				break;

			case XINPUT_GAMEPAD_DPAD_UP:
				std::cout << "XINPUT_GAMEPAD_DPAD_UP\n";
				break;

			case XINPUT_GAMEPAD_LEFT_SHOULDER:
				std::cout << "XINPUT_GAMEPAD_LEFT_SHOULDER\n";
				controller->Vibrate(0X0, 0X0);// 0
				break;

			case XINPUT_GAMEPAD_RIGHT_SHOULDER:
				std::cout << "XINPUT_GAMEPAD_RIGHT_SHOULDER\n";
				controller->Vibrate(0XFFFF, 0XFFFF);// 65535
				break;

			case XINPUT_GAMEPAD_LEFT_THUMB:
				std::cout << "XINPUT_GAMEPAD_LEFT_THUMB\n";
				break;

			case XINPUT_GAMEPAD_RIGHT_THUMB:
				std::cout << "XINPUT_GAMEPAD_RIGHT_THUMB\n";
				break;



			default:
				printf("button %X\n", xinput_state.Gamepad.wButtons);// %x is hexidecimal
			}
		}

		//trigger thresh
		if (xinput_state.Gamepad.bLeftTrigger > 0)
		{
			printf("left trigger %d\n", xinput_state.Gamepad.bLeftTrigger);
		}
		if (xinput_state.Gamepad.bRightTrigger > 0)
		{
			printf("right trigger %d\n", xinput_state.Gamepad.bRightTrigger);
		}
		{
			//joystick deadzone
			if (xinput_state.Gamepad.sThumbLX > 0)
			{				
				ModelMatrix[0] = MMath::rotate(time * 90.0f, Vec3(0.0f, 1.0f, 0.0f));
				ModelMatrix[1] = MMath::rotate(time * 90.0f, Vec3(0.0f, 1.0f, 0.0f));
			}
			else if (xinput_state.Gamepad.sThumbLX < 0)
			{
				ModelMatrix[0] = MMath::rotate(-time * 90.0f, Vec3(0.0f, 1.0f, 0.0f));
				ModelMatrix[1] = MMath::rotate(-time * 90.0f, Vec3(0.0f, 1.0f, 0.0f));
			}
		}
		
	}		
}

void Scene0::soundFunc()
{
	irrklang::ISound* music = AudioEngine->play2D("media/BatmanTheme.wav", false, false, true);
	music->setVolume(0.25f);
}

void Scene0::Update(const float deltaTime) {
	
	time += deltaTime;
	if (!controller->IsConnected())
	{
		//static float elapsedTime = 0.0f;
		//elapsedTime += deltaTime;
		ModelMatrix[0] = MMath::rotate(time * 90.0f, Vec3(0.0f, 1.0f, 0.0f));
		ModelMatrix[1] = MMath::rotate(time * 90.0f, Vec3(0.0f, 1.0f, 0.0f));
	}
}

void Scene0::Render() const {
	PROFILE_FUNCTION();

	switch (renderer->getRendererType()) {

	case RendererType::VULKAN:
		VulkanRenderer* vRenderer;
		vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
		vRenderer->SetCameraUBO(camera->GetProjectionMatrix(), camera->GetViewMatrix());
		vRenderer->SetModelPushConst(ModelMatrix[0]);		
		vRenderer->SetModelPushConst(ModelMatrix[1]);
		vRenderer->SetLightUBO(Vec4(-10.0f, 0.0f, -10.0f, 0.0f), Vec4(10.0f, 0.0f, -10.0f, 0.0f));

		// imgui functions from a UI class or manager?

		vRenderer->Render();
		break;

	case RendererType::OPENGL:
		OpenGLRenderer* glRenderer;
		glRenderer = dynamic_cast<OpenGLRenderer*>(renderer);
		/// Clear the screen
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		/// Draw your scene here
		
		
		glUseProgram(0);
		
		break;
	}
}

void Scene0::OnDestroy() {
	
}
