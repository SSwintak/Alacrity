#include <glew.h>
#pragma once

#define _CRTDBG_MAP_ALLOC

//std
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <utility>
#include <deque>
#include <map>
#include <unordered_map>
#include <chrono>
#include <stdlib.h>  
#include <crtdbg.h>
#include <string>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <set>
#include <array>
#include <string>

//custom
#include "Debug.h"
#include "Matrix.h"
#include "MMath.h"
#include "Plane.h"
#include "Vector.h"
#include "VMath.h"
#include "SceneManager.h"
#include "Timer.h"
//#include "tiny_obj_loader.h"
#include "Renderer.h"
#include "Hash.h"
#include "Sphere.h"
#include "ProfilerTimer.h"

//external dependencies

//VulkanRenderer.h needs these headers +more
/*
#include <SDL.h>
#include <SDL_vulkan.h>
#include <SDL_image.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>
#include <array>
#include <chrono>
*/

#include <SDL.h>
#include <SDL_vulkan.h>
#include <SDL_image.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
