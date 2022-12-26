#ifndef VULKANRENDERER_H 
#define VULKANRENDERER_H

//#include <SDL.h>
//#include <SDL_vulkan.h>
//#include <SDL_image.h>
//#include <vulkan/vulkan.h>
//#include <iostream>
//#include <fstream>
//#include <stdexcept>
//#include <algorithm>
//#include <vector>
//#include <cstring>
//#include <cstdlib>
//#include <cstdint>
//#include <optional>
//#include <set>
//#include <unordered_map>
//#include <array>
//#include <chrono>
//
//#include "Vector.h"
//#include "VMath.h"
//#include "MMath.h"
//#include "Hash.h"

#include "PrecompiledHeader.h"
#include "imgui-docking/imgui.h"


using namespace MATH;


//#include "Renderer.h"


const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG /// only use validation layers if in debug mode
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = false;
#endif

struct QueueFamilyIndices {
    /// optional means that it contains no value until it is assigned
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

    struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};
    
    struct Vertex {
        Vec3 pos;
        Vec3 normal;// alignas(16)
        Vec2 texCoord;// alignas(8)

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, normal);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

            return attributeDescriptions;
        }
        bool operator == (const Vertex& other) const {
            return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
        }
        
    }; /// End of struct Vertex


    namespace std {
        template<> struct hash<Vertex> {
            size_t operator()(Vertex const& vertex) const noexcept {
                size_t hash1 = hash<Vec3>()(vertex.pos);
                size_t hash2 = hash<Vec3>()(vertex.normal);
                size_t hash3 = hash<Vec2>()(vertex.texCoord);
                size_t result = ((hash1 ^ (hash2 << 1)) >> 1) ^ (hash3 << 1);
                return result;
            }
        };
    }

 
struct UniformBufferObject {
    Matrix4 view;
    Matrix4 proj;
    //glm::mat4 view;
    //glm::mat4 proj;
};

struct UniformLightBufferObject {
    Vec4 lightPos[2]{ Vec4(-10.0f, 0.0f, -10.0f, 0.0f), Vec4(10.0f, 0.0f, -10.0f, 0.0f) };
    Vec4 lightColor[2]{ Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0) };
    Vec4 normalColor[2]{ Vec4(0, 1, 1, 0), Vec4(0, 1, 1, 0) };    
};

struct BufferMemory
{    
    VkBuffer bufferID;
    VkDeviceMemory bufferMemoryID;
};

struct ModelMatPushConstants
{  
    Matrix4 render_mat4;
    Matrix4 normal_mat4;
};

struct TextureSampler2D
{
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
};

struct vkSDL_WINDOW
{
    SDL_Window* window;
    unsigned int windowID;
    uint32_t windowWidth;
    uint32_t windowHeight;
};

struct ProfilerResult
{
    const char* Name;
    float Time;
};

class VulkanRenderer : public Renderer {
public:
    /// C11 precautions 
    VulkanRenderer(const VulkanRenderer&) = delete;            /// Copy constructor
    VulkanRenderer(VulkanRenderer&&) = delete;                 /// Move constructor
    VulkanRenderer& operator=(const VulkanRenderer&) = delete; /// Copy operator
    VulkanRenderer& operator=(VulkanRenderer&&) = delete;      /// Move operator

    void SetResize(bool bool_) { framebufferResized = bool_; }
    VulkanRenderer();
    ~VulkanRenderer();
    SDL_Window* CreateAWindow(std::string name_, int width, int height);
    bool OnCreate();
    void OnDestroy();
    void Render();
    void SetCameraUBO(const Matrix4& projection, const Matrix4& view);
    void SetModelPushConst(const Matrix4& model);
    void SetLightUBO(const Vec4& lightPos1, const Vec4& lightPos2);
    SDL_Window* GetWindow() { return window.window; }
    ImDrawData* draw_data;

    void initIMGUI();

private:

    // MAX_FRAMES_IN_FLIGHT = 2
    const size_t MAX_FRAMES_IN_FLIGHT = 2;

    std::vector<ProfilerResult> Profilers;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::vector<std::vector<Vertex>> vertexArray{ vertices, vertices };
    std::vector<std::vector<uint32_t>> indexArray{ indices, indices };

    SDL_Event sdlEvent;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSetLayout textureDescriptorSetLayout;
    VkPipelineLayout pipelineLayout;

    std::array<VkPipeline, 3> graphicsPipelines{ graphicsPipeline, graphicsPipeline, normalsPipeline };
    VkPipeline graphicsPipeline;
    VkPipeline normalsPipeline;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;
    VkDescriptorSet textureDescriptorSet[2];
    
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    VkCommandPool commandPool;

    /*VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;*/

    std::vector<VkBuffer> cameraBuffers;
    std::vector<VkDeviceMemory> cameraBuffersMemory;

    std::vector<VkBuffer> lightUniformBuffers;
    std::vector<VkDeviceMemory> lightUniformBuffersMemory;

    std::vector<std::vector<VkBuffer>> UniformBuffers;
    std::vector<std::vector<VkDeviceMemory>> UniformBuffersMemory;

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;
    VkPipelineCache pipelineCache;
    VkAllocationCallbacks* Allocator;

    bool framebufferResized = false;

    //bool Show_Demo_Window = true;
    //bool show_another_window = false;
    

    bool hasStencilComponent(VkFormat format);

    void initVulkan();
    void createInstance();
    void createSurface();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void recreateSwapChain();

   
    void updateUniformBuffer(uint32_t currentImage);

    void updateLightUniformBuffer(uint32_t currentImage);

    void updateCommandBuffer();

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    void createRenderPass();
    void createDescriptorSetLayout();

    void createGraphicsPipeline(const char* vFilename, const char* fFilename, const char* gFilename, VkPipeline& pipeline, bool push_constants);    

    void createFramebuffers();
    void createCommandPool();
    void createDepthResources();

    void createTextureImage(const char* tFilename, TextureSampler2D& texture);
    void createTextureImageView(TextureSampler2D& texture);
    void createTextureSampler(TextureSampler2D& texture);

    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void loadModel(const char* filename, std::vector<uint32_t>& indexArray_, std::vector<Vertex>& vertexArray_);

    void createVertexBuffer(std::vector<Vertex>& vertexArray_, BufferMemory& vertexBuffer_);

        /// A helper function for createVertexBuffer()
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createIndexBuffer(std::vector<uint32_t>& indexArray_, BufferMemory& indexBuffer_);
    void createUniformBuffers(VkDeviceSize bufferSize, std::vector<VkBuffer>& buffer, std::vector<VkDeviceMemory>& bufferMemory);

    void destroyUniformBuffers(std::vector<VkBuffer>& buffer, std::vector<VkDeviceMemory>& bufferMemory);

    void createDescriptorPool();
    void createDescriptorSets(std::vector<VkBuffer>& UBO, std::vector<VkBuffer>& lights_UBO);
    void createTextureDescriptorSets(TextureSampler2D& textures, VkDescriptorSet& descriptorSet);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createCommandBuffers();
    void createSyncObjects();
    void cleanup();
    void cleanupSwapChain();
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
    void  populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    void setupDebugMessenger();

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    std::vector<const char*> getRequiredExtensions();
    bool checkValidationLayerSupport();

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);    

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    
    UniformBufferObject ubo;
    UniformLightBufferObject LT_ubo;
    ModelMatPushConstants ModelPushConst[2];
    BufferMemory vertexBuffer[2];
    BufferMemory indexBuffer[2];
    vkSDL_WINDOW window;

    TextureSampler2D Tex_Sampler[2];
    std::vector<TextureSampler2D> Tex_Samplers;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    std::vector<ModelMatPushConstants> modelPushBuffers;
    

    VkShaderModule createShaderModule(const std::vector<char>& code);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    static std::vector<char> readFile(const std::string& filename);

   
};
#endif 

