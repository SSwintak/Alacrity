#include "Controller.h"
#include <iostream>

Controller::Controller(int playerID_) :
	controllerID(playerID_)
{
	memset(&controllerState, 0, sizeof(controllerState));
}

bool Controller::IsConnected()
{
	uint16_t result = XInputGetState(controllerID, &controllerState);
	if (result == ERROR_SUCCESS)
		return true;

	if (result == ERROR)
		std::cout << "Controller " << controllerID << " failed to connect!\n";

	return false;
}

XINPUT_STATE Controller::GetControllerState()
{
	return controllerState;
}

void Controller::Vibrate(uint16_t leftVal, uint16_t rightVal)
{
	XINPUT_VIBRATION vibration{};
	vibration.wLeftMotorSpeed = leftVal;
	vibration.wRightMotorSpeed = rightVal;

	if (leftVal || rightVal > 0)
	{
		std::cout << "Vibrating\n";
	}

	XInputSetState(controllerID, &vibration);
}