#include "PrecompiledHeader.h"
#include "VulkanRenderer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "Alacrity/Dependencies/tiny_obj_loader.h"

#include "Alacrity/Dependencies/vendor/imgui-docking/backends/imgui_impl_sdl.h"
#include "Alacrity/Dependencies/vendor/imgui-docking/backends/imgui_impl_vulkan.h"

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

VulkanRenderer::VulkanRenderer(): /// Initialize all the variables
    instance(nullptr), debugMessenger(0), surface(0), commandPool(0), device(nullptr), graphicsPipeline(0),
    Allocator(nullptr), pipelineCache(0), presentQueue(0), graphicsQueue(nullptr), //pipelineLayout(0),
    renderPass(0), swapChain(0), swapChainExtent{},swapChainImageFormat{} {   
     
 }

VulkanRenderer::~VulkanRenderer() {

}

SDL_Window* VulkanRenderer::CreateAWindow(std::string name_, int width_, int height_) {
    window.windowWidth = width_;
    window.windowHeight = height_;
    SDL_Init(SDL_INIT_VIDEO);
    window.window = SDL_CreateWindow(name_.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window.windowWidth, window.windowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);    
    return window.window;
}

bool VulkanRenderer::OnCreate(){   
    PROFILE_FUNCTION();
    initVulkan();        
    initIMGUI();
    return true;
}
void VulkanRenderer::OnDestroy() {
    vkDeviceWaitIdle(device); /// Wait for all commands to clear
    cleanup();
}

void VulkanRenderer::Render() {  
    PROFILE_FUNCTION();
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX); 
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);// is what I'm going to draw valid?, if not rebuild the swapchain... all of it

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();       
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    updateLightUniformBuffer(imageIndex); //needs to be called first
    updateUniformBuffer(imageIndex);
    updateCommandBuffer();// needs to be called here?, ordering matters with imgui, something is still wrong it resizes when hovering over clear color??
    //std::thread th_UpdCommandBuff(&VulkanRenderer::updateCommandBuffer, this); //or std::ref(\*this)?
    //th_UpdCommandBuff.join();// crashes when you hover over or click clear color??
       

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;    

    vkResetFences(device, 1, &inFlightFences[currentFrame]);    

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;
    
    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();       
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;     
}

VkResult VulkanRenderer::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VulkanRenderer::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void VulkanRenderer::initIMGUI()
{
    PROFILE_FUNCTION();
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    io.Fonts->Build();// calls AddFontDefault
    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplSDL2_InitForVulkan(window.window);

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance          = instance;
    init_info.PhysicalDevice    = physicalDevice;
    init_info.Device            = device;
    init_info.QueueFamily       = indices.presentFamily.value();
    init_info.Queue             = presentQueue;
    init_info.PipelineCache     = pipelineCache;
    init_info.DescriptorPool    = imguiPool;
    init_info.Subpass           = 0;
    init_info.MinImageCount     = 2;
    init_info.ImageCount        = MAX_FRAMES_IN_FLIGHT;
    init_info.MSAASamples       = VK_SAMPLE_COUNT_1_BIT;  
    init_info.Allocator         = Allocator;
    init_info.CheckVkResultFn   = check_vk_result;
    

    ImGui_ImplVulkan_Init(&init_info, renderPass);

    VkCommandBuffer cmdBuffer = beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);   
    endSingleTimeCommands(cmdBuffer);    

    vkDeviceWaitIdle(device);
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    
}

void VulkanRenderer::initVulkan() {
    PROFILE_FUNCTION();

    createInstance();
    setupDebugMessenger();
    if (!SDL_Vulkan_CreateSurface(window.window, instance, &surface)) {
        throw std::runtime_error("failed to create window");
    }
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
   
    createGraphicsPipeline("shaders/drawNormals.vert.spv", "shaders/drawNormals.frag.spv", "shaders/drawNormals.geom.spv", graphicsPipelines[2], true);
    createGraphicsPipeline("shaders/phong.vert.spv", "shaders/phong.frag.spv", nullptr, graphicsPipelines[0], true);
    createGraphicsPipeline("shaders/phong.vert.spv", "shaders/phong.frag.spv", nullptr, graphicsPipelines[1], true);
   
    createCommandPool();
    createDepthResources();
    createFramebuffers();

    createTextureImage("./textures/Batman.png", Tex_Sampler[0]);    
    loadModel("./meshes/Batman.obj", indexArray[0], vertexArray[0]);
    createVertexBuffer(vertexArray[0], vertexBuffer[0]);
    createIndexBuffer(indexArray[0], indexBuffer[0]);

    createTextureImage("./textures/skull_texture32bit.png", Tex_Sampler[1]);
    loadModel("./meshes/Skull.obj", indexArray[1], vertexArray[1]);
    createVertexBuffer(vertexArray[1], vertexBuffer[1]);
    createIndexBuffer(indexArray[1], indexBuffer[1]);

    createUniformBuffers(sizeof(UniformBufferObject), cameraBuffers, cameraBuffersMemory);
    createUniformBuffers(sizeof(UniformLightBufferObject), lightUniformBuffers, lightUniformBuffersMemory);

    createDescriptorPool();
    createDescriptorSets(cameraBuffers, lightUniformBuffers);
    createTextureDescriptorSets(Tex_Samplers[0], textureDescriptorSet[0]);
    createTextureDescriptorSets(Tex_Samplers[1], textureDescriptorSet[1]);

    createCommandBuffers();
    createSyncObjects();    
}

void VulkanRenderer::cleanupSwapChain() {
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

    for (int i = 0; i < graphicsPipelines.size(); ++i)
    {
        vkDestroyPipeline(device, graphicsPipelines[i], nullptr);
    }
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);    
    vkDestroyRenderPass(device, renderPass, nullptr);

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);

    destroyUniformBuffers(cameraBuffers, cameraBuffersMemory);
    destroyUniformBuffers(lightUniformBuffers, lightUniformBuffersMemory);

    vkDestroyDescriptorPool(device, descriptorPool, Allocator);
}

void VulkanRenderer::destroyUniformBuffers(std::vector<VkBuffer>& buffer, std::vector<VkDeviceMemory>& bufferMemory)
{
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        vkDestroyBuffer(device, buffer[i], nullptr);
        vkFreeMemory(device, bufferMemory[i], nullptr);
    }
}

void VulkanRenderer::cleanup() {

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    cleanupSwapChain();    
    
    for (int i = 0; i < Tex_Samplers.size(); ++i)
    {
        vkDestroySampler(device, Tex_Samplers[i].textureSampler, nullptr);
        vkDestroyImageView(device, Tex_Samplers[i].textureImageView, nullptr);

        vkDestroyImage(device, Tex_Samplers[i].textureImage, nullptr);
        vkFreeMemory(device, Tex_Samplers[i].textureImageMemory, nullptr);
    }
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, textureDescriptorSetLayout, nullptr);

    for (int i = 0; i < 2; ++i)
    {
        vkDestroyBuffer(device, indexBuffer[i].bufferID, nullptr);
        vkFreeMemory(device, indexBuffer[i].bufferMemoryID, nullptr);

        vkDestroyBuffer(device, vertexBuffer[i].bufferID, nullptr);
        vkFreeMemory(device, vertexBuffer[i].bufferMemoryID, nullptr);
    }    

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyDevice(device, Allocator);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, Allocator);

    SDL_DestroyWindow(window.window);
    window.window = nullptr;
}

void VulkanRenderer::recreateSwapChain() {
    int width = 0, height = 0;

    SDL_GetWindowSize(window.window, &width, &height);

    while (width == 0 || height == 0) {
        SDL_GetWindowSize(window.window, &width, &height);
        SDL_WaitEvent(&sdlEvent);
    }
    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createRenderPass();
    
    createGraphicsPipeline("shaders/drawNormals.vert.spv", "shaders/drawNormals.frag.spv", "/shaders/drawNormals.geom.spv", graphicsPipelines[2], true);
    createGraphicsPipeline("shaders/phong.vert.spv", "shaders/phong.frag.spv", nullptr, graphicsPipelines[0], true);
    createGraphicsPipeline("shaders/phong.vert.spv", "shaders/phong.frag.spv", nullptr, graphicsPipelines[1], true);
    
    createDepthResources();
    createFramebuffers();
    createUniformBuffers(sizeof(UniformBufferObject), cameraBuffers, cameraBuffersMemory);
    createUniformBuffers(sizeof(UniformLightBufferObject), lightUniformBuffers, lightUniformBuffersMemory);
    createDescriptorPool();
    createDescriptorSets(cameraBuffers, lightUniformBuffers);
    createTextureDescriptorSets(Tex_Samplers[0], textureDescriptorSet[0]);
    createTextureDescriptorSets(Tex_Samplers[1], textureDescriptorSet[1]);
    createCommandBuffers();
    
}

void VulkanRenderer::createInstance() {
    PROFILE_FUNCTION();
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, Allocator, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void VulkanRenderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (!CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void VulkanRenderer::createSurface() {
    if (!SDL_Vulkan_CreateSurface(window.window, instance, &surface)) {
        throw std::runtime_error("failed to create window");
    }
}

void VulkanRenderer::pickPhysicalDevice() {
    PROFILE_FUNCTION();
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void VulkanRenderer::createLogicalDevice() {
    PROFILE_FUNCTION();
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, Allocator, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanRenderer::createSwapChain() {
    PROFILE_FUNCTION();
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VulkanRenderer::createImageViews() {
    PROFILE_FUNCTION();
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanRenderer::createRenderPass() {
    PROFILE_FUNCTION();
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VulkanRenderer::createDescriptorSetLayout() {
    PROFILE_FUNCTION();
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding uboLayoutLightBinding{};
    uboLayoutLightBinding.binding = 1;
    uboLayoutLightBinding.descriptorCount = 1;
    uboLayoutLightBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutLightBinding.pImmutableSamplers = nullptr;
    uboLayoutLightBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutLightBinding, uboLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create uniform descriptor set layout!");
    }

    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &textureDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture descriptor set layout!");
    }
    
}

void VulkanRenderer::createGraphicsPipeline(const char* vFilename, const char* fFilename, const char* gFilename, VkPipeline& pipeline, bool push_constants) {
    PROFILE_FUNCTION();
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    
    auto vertShaderCode = readFile(vFilename);
    auto fragShaderCode = readFile(fFilename);

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    shaderStages.push_back(vertShaderStageInfo);
    shaderStages.push_back(fragShaderStageInfo);
   
    VkShaderModule geomShaderModule;
    if (gFilename)
    {
        auto geomShaderCode = readFile(gFilename);
        geomShaderModule = createShaderModule(geomShaderCode);
        VkPipelineShaderStageCreateInfo geomShaderStageInfo{};
        geomShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        geomShaderStageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        geomShaderStageInfo.module = geomShaderModule;
        geomShaderStageInfo.pName = "main";
        shaderStages.push_back(geomShaderStageInfo);
    }

    //VkPipelineShaderStageCreateInfo shaderStages2[] = { vertShaderStageInfo, fragShaderStageInfo };
    

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};

    if (push_constants)
    {
        //setup push constants
        VkPushConstantRange push_constant;
        //this push constant range starts at the beginning
        push_constant.offset = 0;
        //this push constant range takes up the size of a MeshPushConstants struct
        push_constant.size = sizeof(ModelMatPushConstants);
        //this push constant range is accessible only in the vertex shader
        push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        pipelineLayoutInfo.pPushConstantRanges = &push_constant;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

    VkDescriptorSetLayout setLayouts[] = { descriptorSetLayout, textureDescriptorSetLayout };

    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = setLayouts;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;    
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
   
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    if(gFilename)
        vkDestroyShaderModule(device, geomShaderModule, nullptr);
}

void VulkanRenderer::createFramebuffers() {
    PROFILE_FUNCTION();
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void VulkanRenderer::createCommandPool() {
    PROFILE_FUNCTION();
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics command pool!");
    }
}

void VulkanRenderer::createDepthResources() {
    PROFILE_FUNCTION();
    VkFormat depthFormat = findDepthFormat();

    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

VkFormat VulkanRenderer::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool VulkanRenderer::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanRenderer::createTextureImage(const char* tFilename, TextureSampler2D& texture) {
    PROFILE_FUNCTION();
    SDL_Surface* image = IMG_Load(tFilename);//Vulkan won't accept images under 32 bit depth , mario_main32bit.png
    ///image->format
    VkDeviceSize imageSize = image->w * image->h * 4;// forcing rgba alignment


    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, image->pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    // convert to a normalized image to use in UV coords
    createImage(image->w, image->h, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, 
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.textureImage, texture.textureImageMemory);

    transitionImageLayout(texture.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, texture.textureImage, static_cast<uint32_t>(image->w), static_cast<uint32_t>(image->h));
    transitionImageLayout(texture.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    SDL_FreeSurface(image);

    createTextureImageView(texture);
    createTextureSampler(texture);
    Tex_Samplers.push_back(texture);
}

void VulkanRenderer::createTextureImageView(TextureSampler2D& texture) {
    texture.textureImageView = createImageView(texture.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanRenderer::createTextureSampler(TextureSampler2D& texture) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

void VulkanRenderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

void VulkanRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void VulkanRenderer::loadModel(const char* filename, std::vector<uint32_t>& indexArray_, std::vector<Vertex>& vertexArray_) {
    PROFILE_FUNCTION();
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;    

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename)) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],//0, 1, 2 is x y z
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };

            //count is overloaded to use hash, it compares the verticies hash ID if its 1 
            //they are the same throw it away, if 0 its new store it
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertexArray_.size());
                vertexArray_.push_back(vertex);
            }

            indexArray_.push_back(uniqueVertices[vertex]);
        }
    }
}

void VulkanRenderer::createVertexBuffer(std::vector<Vertex>& vertexArray_, BufferMemory& vertexBuffer_) {
    PROFILE_FUNCTION();
    VkDeviceSize bufferSize = sizeof(vertexArray_[0]) * vertexArray_.size();

    BufferMemory stagingBuffer;

    /*VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;*/
                                                                                                                       // these are being pulled in by reference, be constatant
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.bufferID, stagingBuffer.bufferMemoryID);

    void* data;
    vkMapMemory(device, stagingBuffer.bufferMemoryID, 0, bufferSize, 0, &data);// GPU loads the staging memory with this much memory (buffersize)
    memcpy(data, vertexArray_.data(), (size_t)bufferSize);//copy all the data in the pointer/GPU
    vkUnmapMemory(device, stagingBuffer.bufferMemoryID);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer_.bufferID, vertexBuffer_.bufferMemoryID);

    copyBuffer(stagingBuffer.bufferID, vertexBuffer_.bufferID, bufferSize);

    vkDestroyBuffer(device, stagingBuffer.bufferID, nullptr);
    vkFreeMemory(device, stagingBuffer.bufferMemoryID, nullptr);
}

void VulkanRenderer::createIndexBuffer(std::vector<uint32_t>& indexArray_, BufferMemory& indexBuffer_) {
    PROFILE_FUNCTION();
    VkDeviceSize bufferSize = sizeof(uint32_t) * indexArray_.size();

    BufferMemory stagingBuffer;

    /*VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;*/

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.bufferID, stagingBuffer.bufferMemoryID);

    void* data;
    vkMapMemory(device, stagingBuffer.bufferMemoryID, 0, bufferSize, 0, &data);
    memcpy(data, indexArray_.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBuffer.bufferMemoryID);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer_.bufferID, indexBuffer_.bufferMemoryID);

    copyBuffer(stagingBuffer.bufferID, indexBuffer_.bufferID, bufferSize);

    vkDestroyBuffer(device, stagingBuffer.bufferID, nullptr);
    vkFreeMemory(device, stagingBuffer.bufferMemoryID, nullptr);
}

void VulkanRenderer::createUniformBuffers(VkDeviceSize bufferSize, std::vector<VkBuffer>& buffer, std::vector<VkDeviceMemory>& bufferMemory) {
    PROFILE_FUNCTION();
    buffer.resize(swapChainImages.size());
    bufferMemory.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer[i], bufferMemory[i]);
        
    }
}

void VulkanRenderer::createDescriptorPool() {
    PROFILE_FUNCTION();
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(swapChainImages.size());
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = static_cast<uint32_t>(swapChainImages.size());      

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(swapChainImages.size()) + 3;

    if (vkCreateDescriptorPool(device, &poolInfo, Allocator, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void VulkanRenderer::createTextureDescriptorSets(TextureSampler2D& textures, VkDescriptorSet& descriptorSet) {

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &textureDescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textures.textureImageView;
    imageInfo.sampler = textures.textureSampler;
    
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

}

void VulkanRenderer::createDescriptorSets(std::vector<VkBuffer>& UBO, std::vector<VkBuffer>& lights_UBO) {
    PROFILE_FUNCTION();
    std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(swapChainImages.size());
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }
   
    for (size_t i = 0; i < swapChainImages.size(); i++) {

        VkDescriptorBufferInfo cameraBufferInfo{};
        cameraBufferInfo.buffer = UBO[i];
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.buffer = lights_UBO[i];
        lightBufferInfo.offset = 0;// need an offset for every other UBO + the other offsets if more than two UBOs
        lightBufferInfo.range = sizeof(UniformLightBufferObject);        

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &cameraBufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &lightBufferInfo;        

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);        
    }
}

void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; 

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkCommandBuffer VulkanRenderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanRenderer::createCommandBuffers() {
    PROFILE_FUNCTION();
    commandBuffers.resize(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }    
}

void VulkanRenderer::createSyncObjects() {
    PROFILE_FUNCTION();
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void VulkanRenderer::SetCameraUBO(const Matrix4& projection, const Matrix4& view) {
    PROFILE_FUNCTION();
    ubo.proj = projection;
    ubo.proj[5] *= -1.0f;// glm mat4 would be [1][1] instead of [5], Scott overloaded the operator[] 

    ubo.view = view;    
}

void VulkanRenderer::SetModelPushConst(const Matrix4& model)
{
    PROFILE_FUNCTION();
    //PROFILE_SCOPE("SetModel PushConstant");
    ModelPushConst[0].render_mat4 = model;
    ModelPushConst[0].normal_mat4 = MMath::transpose(MMath::inverse(ModelPushConst[0].render_mat4));

    //ModelPushConst[1].render_mat4 = model * MMath::translate(4.0f, 0.0f, 0.0f);// rotates along the first models origin
    ModelPushConst[1].render_mat4 = MMath::translate(-6.0f, 0.0f, (6.0f * -1.0f)) * model;// rotates along its own origin
    ModelPushConst[1].normal_mat4 = MMath::transpose(MMath::inverse(ModelPushConst[1].render_mat4));        
}

void VulkanRenderer::SetLightUBO(const Vec4& lightPos1, const Vec4& lightPos2)
{
    PROFILE_FUNCTION();
    //Vec4(-12.0f, 0.0f, -15.0f, 0.0f)
    //Vec4(12.0f, 0.0f, -15.0f, 0.0f)
    //LT_ubo.lightPos[0] = lightPos1;
    //LT_ubo.lightPos[1] = lightPos2;       
}


void VulkanRenderer::updateCommandBuffer()
{

    //PROFILE_SCOPE("updateCommandBuffer");
    // these probably shouldn't be static
    static bool Show_Demo_Window = false;
    static bool show_another_window = false;
    static bool show_normals = false;    

    for (size_t i = 0; i < commandBuffers.size(); i++) {
        VkRenderPassBeginInfo renderPassInfo{};
        // render pass prep
        {
            PROFILE_SCOPE("updateCommandBuffer RenderPass Prep");
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            // begin recording the command buffer
            if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[i];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = swapChainExtent;

            // background color
            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color.float32[0] = clear_color.x * clear_color.w;
            clearValues[0].color.float32[1] = clear_color.y * clear_color.w;
            clearValues[0].color.float32[2] = clear_color.z * clear_color.w;
            clearValues[0].color.float32[3] = clear_color.w;
            clearValues[1].depthStencil = { 1.0f, 0 };

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();
        }

        // Cmd binds
        {
            PROFILE_SCOPE("updateCommandBuffer Cmd Binds");
            // choose pipeline and bind it
            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[0]);

            // bind the vert and index data
            VkBuffer vertexBuffers[] = { vertexBuffer[0].bufferID, vertexBuffer[1].bufferID };
            
            VkDeviceSize offsets[] = { 0 };

            // could incapsulate in a loop?
            // for(auto models : modelPushBuffers)        

            // first model, Batman
            vkCmdPushConstants(commandBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatPushConstants), &ModelPushConst[0]);// i?

            vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &vertexBuffers[0], offsets);
            vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer[0].bufferID, 0, VK_INDEX_TYPE_UINT32);

            //UBO descriptor
            vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);

            //Texture descriptor
            vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &textureDescriptorSet[0], 0, nullptr);

            vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indexArray[0].size()), 1, 0, 0, 0);

            if (show_normals)
            {
                vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[2]);
                vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indexArray[0].size()), 1, 0, 0, 0);
            }

            // second model, Skull
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[1]);// superflous, unless you have multiple pipelines
            vkCmdPushConstants(commandBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatPushConstants), &ModelPushConst[1]);// only allowed one push constant per pipeline, but you can change the data in it

            vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &vertexBuffers[1], offsets);
            vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer[1].bufferID, 0, VK_INDEX_TYPE_UINT32);

            //UBO descriptor
            vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
            //Texture descriptor
            vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &textureDescriptorSet[1], 0, nullptr);

            vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indexArray[1].size()), 1, 0, 0, 0);

            if (show_normals)
            {
                vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelines[2]);
                vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indexArray[1].size()), 1, 0, 0, 0);
            }
        }

        // imgui window creation
        {
            PROFILE_SCOPE("updateCommandBuffer Imgui Window Creation");
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // for initial testing
            if (Show_Demo_Window)
            {
                ImGui::ShowDemoWindow(&Show_Demo_Window);
                //ImGui::SetWindowSize(ImVec2(100, 100));
            }
            static float f = 0.0f;
            static int counter = 0;

            // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
            {
                static float f = 0.0f;
                static int counter = 0;

                ImGui::Begin("Main Window");                              // Create a window called "Hello, world!" and append into it.

                ImGui::Text("The Demo Window Increases compute time... ugh.");                 // Display some text (you can use a format strings too)
                ImGui::Checkbox("Demo Window", &Show_Demo_Window);        // Edit bools storing our window open/close state
                ImGui::Checkbox("Another Window", &show_another_window);
                ImGui::Checkbox("Normals On/Off", &show_normals);
                //ImGui::Checkbox("Batman/Skull", &);

                ImGui::Separator();

                ImGui::SliderFloat("Model Position", &f, 0.0f, 1.0f);     // Edit 1 float using a slider from 0.0f to 1.0f
                ImGui::ColorEdit3("clear color", (float*)(&clear_color)); // Edit 3 floats representing a color
                ImGui::ColorEdit3("light1 color", (float*)(&LT_ubo.lightColor[0]));
                ImGui::ColorEdit3("light2 color", (float*)(&LT_ubo.lightColor[1]));
                ImGui::SliderFloat3("lightPos1", (float*)&LT_ubo.lightPos[0], -50.0f, 50.0f);
                ImGui::SliderFloat3("lightPos2", (float*)&LT_ubo.lightPos[1], -50.0f, 50.0f);
                ImGui::ColorEdit3("normal color1", (float*)(&LT_ubo.normalColor[0]));
                                
                ImGui::Separator();
                
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::End();

                ImGui::Begin("Profiler");

                for (auto& result : Profilers)
                {
                    /*char label[50];
                    strcpy_s(label, result.Name);
                    strcat_s(label, " %.3fms");*/
                    ImGui::Text("%s %.3fms", result.Name ,result.Time);
                }
                Profilers.clear();

                ImGui::End();
            }

            // 3. Show another simple window.
            if (show_another_window)
            {
                ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
                ImGui::Text("Hello from another window!");
                if (ImGui::Button("Close Me"))
                    show_another_window = false;
                ImGui::End();
            }
        }

        //Imgui Render
        {
            PROFILE_SCOPE("updateCommandBuffer Imgui Render");

            ImGui::Render();
            draw_data = ImGui::GetDrawData();
            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }

            ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffers[i]);
        }
        vkCmdEndRenderPass(commandBuffers[i]);

        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }        
    }
}

void VulkanRenderer::updateUniformBuffer(uint32_t currentImage) {
  
    
    void* data;// get a pointer into the staging memory of the GPU
    vkMapMemory(device, cameraBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);// align the memory
    memcpy(data, &ubo, sizeof(ubo));// copy the ubo memory into the GPU staging memory
    vkUnmapMemory(device, cameraBuffersMemory[currentImage]);

}

void VulkanRenderer::updateLightUniformBuffer(uint32_t currentImage)
{
    //light UBO
    void* lightData;
    vkMapMemory(device, lightUniformBuffersMemory[currentImage], 0, sizeof(LT_ubo), 0, &lightData);
    memcpy(lightData, &LT_ubo, sizeof(LT_ubo));
    vkUnmapMemory(device, lightUniformBuffersMemory[currentImage]);
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        VkExtent2D actualExtent = { window.windowWidth, window.windowHeight };
        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
        return actualExtent;
    }
}

SwapChainSupportDetails VulkanRenderer::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool VulkanRenderer::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices VulkanRenderer::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions() {
    unsigned int extensionCount;
    if (!SDL_Vulkan_GetInstanceExtensions(window.window, &extensionCount, nullptr))
        throw std::runtime_error("failed to get required SDL extension count!");

    std::vector<const char*> extensions(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window.window, &extensionCount, extensions.data()))
        throw std::runtime_error("failed to get required SDL extensions!");

    if (enableValidationLayers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

bool VulkanRenderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

std::vector<char> VulkanRenderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        printf("File %s cannot open!\n", filename.c_str());
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}
