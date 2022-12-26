#ifndef OPENGLRENDERER_H
#define OPENGLRENDERER_H
#include "Renderer.h"
class OpenGLRenderer: public Renderer {
public:
	OpenGLRenderer();
	~OpenGLRenderer();
	virtual SDL_Window* CreateAWindow(std::string name_, int width_, int height_);
	SDL_Window* GetWindow() { return window; }
	virtual bool OnCreate();
	virtual void OnDestroy();
	virtual void Render();
private:
	int width, height;
	SDL_Window* window;
	SDL_GLContext context;
	void setAttributes(int major_, int minor_);
	void getInstalledOpenGLInfo(int* major, int* minor);

};

#endif
