#ifndef SCENE0_H
#define SCENE0_H
#include "Scene.h"
#include "Vector.h"
#include "Renderer.h"
#include "Camera.h"
#include "MovableCamera.h"
#include <irrKlang.h>
#include "UIManager.h"
#include "Controller.h"
#include <unordered_map>

using namespace MATH;

/// Forward declarations 
union SDL_Event;
class Actor;

class Scene0 : public Scene {
private:
	Renderer *renderer;
	Camera *camera;
	MovableCamera* movableCamera;
	Matrix4 ModelMatrix[2];
	
	
	//ModelName, Model (WIP)
	std::unordered_map<const char*, Matrix4> models;

	irrklang::ISoundEngine* AudioEngine;
	UIManager* UI;
	Controller* controller;
	float time = 0.0f;
	

public:
	explicit Scene0(Renderer* renderer_);
	virtual ~Scene0();

	virtual bool OnCreate() override;
	virtual void OnDestroy() override;
	virtual void Update(const float deltaTime) override;
	virtual void Render() const override;
	virtual void HandleEvents(const SDL_Event &sdlEvent) override;
	void soundFunc();

	//uses XINPUT
	virtual void GetControllerEvents() override;
};


#endif // SCENE0_H