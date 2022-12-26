#include <stdlib.h>  
#include <crtdbg.h>
#include <string>
#include "SceneManager.h"
#include "Debug.h"
#include "MMath.h"

using namespace MATH;

size_t Alloc_Memory[2];
int allocations[2];

void* operator new(std::size_t amount)
{
	++allocations[0];
	Alloc_Memory[0] += amount;

	allocations[1] = allocations[0];
	Alloc_Memory[1] = Alloc_Memory[0];
	
	return malloc(amount);	
}

void operator delete(void* memLoc, std::size_t amount)
{
	--allocations[0];
	Alloc_Memory[0] -= amount;

	if (Alloc_Memory[0] < 100)
	{
		std::cout << typeid(amount).name() << ": FREEING " << amount << " Bytes of Memory\n";
		std::cout << "Total Memory: " << Alloc_Memory[0] << " Bytes\n";
		std::cout << "Total Allocations: " << allocations[0] << "\n";
	}

	free(memLoc);

}

int main(int argc, char* args[]) {
/***	
#pragma warning(disable : 4996) 
	if (const char* env_p = std::getenv("PATH")) {
		std::cout << "Your PATH is: " << env_p << '\n';
	}
***/
	
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	{
		const std::string name = { "Graphics Game Engine" };
		Debug::DebugInit(name + "_Log");
		Debug::Info("Starting the GameSceneManager", __FILE__, __LINE__);

		SceneManager* gsm = new SceneManager();
		if (gsm->Initialize(name, 1280, 720, RendererType::VULKAN) == true) {
			gsm->Run();
			std::cout << "Total Memory Before Delete: " << Alloc_Memory[1] << " Bytes\n";
			std::cout << "Total Allocations Before Delete: " << allocations[1] << "\n";
			std::cout << "Total Memory: " << Alloc_Memory[0] << " Bytes\n";
			std::cout << "Total Allocations: " << allocations[0] << "\n";
		}


		delete gsm;
	}
	
	//std::this_thread::sleep_for(std::chrono::seconds(1));
	//_CrtDumpMemoryLeaks();
	exit(0);

}
