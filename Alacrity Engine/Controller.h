#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


#include <Xinput.h>
#include <cstdint>

class Controller
{

	int controllerID;
	XINPUT_STATE controllerState;


public:

	bool IsConnected();
	void Vibrate(uint16_t leftVal, uint16_t rightVal);


	Controller(int playerID_);
	XINPUT_STATE GetControllerState();	

};

