#include "../RenderPCH.h"
#include <windows.h> // Ensure windows.h is included for types
#include <stdexcept>
#include <cstring>
#include "VulkanRenderer.h"
#include "VulkanInternalShaders.h"
#include <IConsole.h>
#include <ILog.h>
#include <set>
#include <algorithm>
#include <array>

CVulkanRenderer *gcpVulkan = NULL;

static VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

static VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice) {
    return FindSupportedFormat(
        physicalDevice,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

const int MAX_FRAMES_IN_FLIGHT = 2;

// --------------------------------------------------------------------------------
// Vulkan Renderer Implementation
// --------------------------------------------------------------------------------

CVulkanRenderer::CVulkanRenderer()
    : m_Instance(VK_NULL_HANDLE)
    , m_PhysicalDevice(VK_NULL_HANDLE)
    , m_Device(VK_NULL_HANDLE)
    , m_Queue(VK_NULL_HANDLE)
    , m_Surface(VK_NULL_HANDLE)
    , m_Swapchain(VK_NULL_HANDLE)
    , m_CommandPool(VK_NULL_HANDLE)
    , m_PipelineCache(VK_NULL_HANDLE)
    , m_VS2D(VK_NULL_HANDLE)
    , m_PS2D(VK_NULL_HANDLE)
    , m_CurrentFrame(0)
    , m_ImageIndex(0)
    , m_DynVBOffset(0)
    , m_DynVBSize(0)
{
    if (!gcpVulkan)
        gcpVulkan = this;

    m_type = R_VULKAN_RENDERER;

    m_TexMan = new CVKTexMan;
}

CVulkanRenderer::~CVulkanRenderer()
{
    ShutDown();
    gcpVulkan = NULL;
    delete m_TexMan;
}

#ifndef PS2
WIN_HWND CVulkanRenderer::Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen,WIN_HINSTANCE hinst, WIN_HWND Glhwnd, WIN_HDC Glhdc, WIN_HGLRC hGLrc, bool bReInit)
{
    m_width = width;
    m_height = height;
    m_cbpp = cbpp;
    m_zbpp = zbpp;
    m_sbpp = sbits;
    m_FullScreen = fullscreen;
    m_hWnd = Glhwnd;

    if (bReInit)
        return m_hWnd;

    if (m_pLog) m_pLog->Log("Initializing Vulkan Renderer...");

    try {
        CreateInstance();
        CreateSurface(hinst, Glhwnd);
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapchain();
        CreateImageViews();
        CreateRenderPass();
        CreateDepthResources();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffers();
        CreateSyncObjects();

        // Create Dynamic Vertex Buffer
        m_DynVBSize = 4 * 1024 * 1024; // 4MB
        m_DynVBs.resize(MAX_FRAMES_IN_FLIGHT);
        m_DynVBMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            CreateVulkanBuffer(m_DynVBSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_DynVBs[i].buffer, m_DynVBs[i].memory);
            m_DynVBs[i].size = m_DynVBSize;
            vkMapMemory(m_Device, m_DynVBs[i].memory, 0, m_DynVBSize, 0, &m_DynVBMapped[i]);
        }

        // Create Internal Shaders
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = sizeof(VS_2D);
        createInfo.pCode = VS_2D;
        if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &m_VS2D) != VK_SUCCESS)
             if (m_pLog) m_pLog->Log("Failed to create VS_2D");

        createInfo.codeSize = sizeof(PS_2D);
        createInfo.pCode = PS_2D;
        if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &m_PS2D) != VK_SUCCESS)
             if (m_pLog) m_pLog->Log("Failed to create PS_2D");

    } catch (const std::exception& e) {
        if (m_pLog) m_pLog->Log("Vulkan Initialization Failed: %s", e.what());
        ShutDown();
        return 0;
    }

    if (m_pLog) m_pLog->Log("Vulkan Renderer Initialized Successfully");

    return m_hWnd;
}
#else
bool CVulkanRenderer::Init(int x,int y,int width,int height,unsigned int cbpp, int zbpp, int sbits, bool fullscreen, bool bReInit)
{
    return true;
}
#endif

void CVulkanRenderer::ShutDown(bool bReInit)
{
    if (m_pLog) m_pLog->Log("Shutting down Vulkan Renderer...");

    if (m_Device) {
        vkDeviceWaitIdle(m_Device);
    }

    for (size_t i = 0; i < m_RenderFinishedSemaphores.size(); i++) {
        vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
    }
    m_RenderFinishedSemaphores.clear();

    for (size_t i = 0; i < m_ImageAvailableSemaphores.size(); i++) {
        vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
    }
    m_ImageAvailableSemaphores.clear();

    for (size_t i = 0; i < m_InFlightFences.size(); i++) {
        vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
    }
    m_InFlightFences.clear();

    for (size_t i = 0; i < m_DynVBs.size(); i++) {
        if (m_DynVBMapped[i]) { vkUnmapMemory(m_Device, m_DynVBs[i].memory); }
        if (m_DynVBs[i].buffer) { vkDestroyBuffer(m_Device, m_DynVBs[i].buffer, nullptr); }
        if (m_DynVBs[i].memory) { vkFreeMemory(m_Device, m_DynVBs[i].memory, nullptr); }
    }
    m_DynVBs.clear();
    m_DynVBMapped.clear();

    if (m_CommandPool) {
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;
    }

    if (m_PipelineCache) {
        vkDestroyPipelineCache(m_Device, m_PipelineCache, nullptr);
        m_PipelineCache = VK_NULL_HANDLE;
    }

    if (m_VS2D) vkDestroyShaderModule(m_Device, m_VS2D, nullptr);
    if (m_PS2D) vkDestroyShaderModule(m_Device, m_PS2D, nullptr);

    for (auto const& [key, val] : m_Pipelines) {
        delete val;
    }
    m_Pipelines.clear();

    for (auto framebuffer : m_SwapchainFramebuffers) {
        vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
    }
    m_SwapchainFramebuffers.clear();

    if (m_RenderPass) {
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }

    if (m_DepthImageView) { vkDestroyImageView(m_Device, m_DepthImageView, nullptr); m_DepthImageView = VK_NULL_HANDLE; }
    if (m_DepthImage) { vkDestroyImage(m_Device, m_DepthImage, nullptr); m_DepthImage = VK_NULL_HANDLE; }
    if (m_DepthImageMemory) { vkFreeMemory(m_Device, m_DepthImageMemory, nullptr); m_DepthImageMemory = VK_NULL_HANDLE; }

    for (auto imageView : m_SwapchainImageViews) {
        vkDestroyImageView(m_Device, imageView, nullptr);
    }
    m_SwapchainImageViews.clear();

    if (m_Swapchain) {
        vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }

    if (m_Device) {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }

    if (m_Surface) {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }

    if (m_Instance) {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
}

void CVulkanRenderer::BeginFrame()
{
    vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

    VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_ImageIndex);

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_SwapchainFramebuffers[m_ImageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_SwapchainExtent;

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_SwapchainExtent.width;
    viewport.height = (float)m_SwapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_CommandBuffers[m_CurrentFrame], 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = m_SwapchainExtent;
    vkCmdSetScissor(m_CommandBuffers[m_CurrentFrame], 0, 1, &scissor);

    m_DynVBOffset = 0;
}

void CVulkanRenderer::Update()
{
    vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);

    if (vkEndCommandBuffer(m_CommandBuffers[m_CurrentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphores[m_CurrentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

    VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[m_CurrentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_Queue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {m_Swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &m_ImageIndex;
    presentInfo.pResults = nullptr;

    vkQueuePresentKHR(m_Queue, &presentInfo);

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    m_TexMan->Update();
}

void CVulkanRenderer::CreateInstance()
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "CryEngine Game";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "CryEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void CVulkanRenderer::CreateSurface(WIN_HINSTANCE hinst, WIN_HWND hWnd)
{
    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = (HINSTANCE)hinst;
    createInfo.hwnd = (HWND)hWnd;

    if (vkCreateWin32SurfaceKHR(m_Instance, &createInfo, nullptr, &m_Surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void CVulkanRenderer::PickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (IsDeviceSuitable(device)) {
            m_PhysicalDevice = device;
            break;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool CVulkanRenderer::IsDeviceSuitable(VkPhysicalDevice device)
{
    bool extensionsSupported = CheckDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    return extensionsSupported && swapChainAdequate;
}

bool CVulkanRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions;
    requiredExtensions.insert(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

CVulkanRenderer::SwapChainSupportDetails CVulkanRenderer::QuerySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void CVulkanRenderer::CreateLogicalDevice()
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    int graphicsQueueFamilyIndex = -1;
    for (int i = 0; i < (int)queueFamilies.size(); i++) {
        if (queueFamilies[i].queueCount > 0 && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphicsQueueFamilyIndex = i;
            break;
        }
    }

    if (graphicsQueueFamilyIndex == -1) {
        throw std::runtime_error("failed to find a graphics queue family!");
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures = {};

    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(m_Device, 0, 0, &m_Queue);

    // Create Pipeline Cache
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (vkCreatePipelineCache(m_Device, &pipelineCacheCreateInfo, nullptr, &m_PipelineCache) != VK_SUCCESS) {
        if (m_pLog) m_pLog->Log("Failed to create pipeline cache!");
    }
}

void CVulkanRenderer::CreateSwapchain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

    VkSurfaceFormatKHR surfaceFormat = swapChainSupport.formats[0];
    for (const auto& availableFormat : swapChainSupport.formats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = availableFormat;
            break;
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync
    for (const auto& availablePresentMode : swapChainSupport.presentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = availablePresentMode;
            break;
        }
    }

    VkExtent2D extent = swapChainSupport.capabilities.currentExtent;
    if (swapChainSupport.capabilities.currentExtent.width == UINT32_MAX) {
        extent.width = (std::max)(swapChainSupport.capabilities.minImageExtent.width, (std::min)(swapChainSupport.capabilities.maxImageExtent.width, (uint32_t)m_width));
        extent.height = (std::max)(swapChainSupport.capabilities.minImageExtent.height, (std::min)(swapChainSupport.capabilities.maxImageExtent.height, (uint32_t)m_height));
    }

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_Surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
    m_SwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

    m_SwapchainImageFormat = surfaceFormat.format;
    m_SwapchainExtent = extent;
}

void CVulkanRenderer::CreateImageViews() {
    m_SwapchainImageViews.resize(m_SwapchainImages.size());

    for (size_t i = 0; i < m_SwapchainImages.size(); i++) {
        m_SwapchainImageViews[i] = CreateImageView(m_SwapchainImages[i], m_SwapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void CVulkanRenderer::CreateRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = m_SwapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear screen
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = FindDepthFormat(m_PhysicalDevice);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void CVulkanRenderer::CreateDepthResources() {
    VkFormat depthFormat = FindDepthFormat(m_PhysicalDevice);
    CreateImage(m_SwapchainExtent.width, m_SwapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DepthImage, m_DepthImageMemory);
    m_DepthImageView = CreateImageView(m_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void CVulkanRenderer::CreateFramebuffers() {
    m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());

    for (size_t i = 0; i < m_SwapchainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            m_SwapchainImageViews[i],
            m_DepthImageView
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_SwapchainExtent.width;
        framebufferInfo.height = m_SwapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapchainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void CVulkanRenderer::CreateCommandPool()
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    int graphicsQueueFamilyIndex = -1;
    for (int i = 0; i < (int)queueFamilies.size(); i++) {
        if (queueFamilies[i].queueCount > 0 && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphicsQueueFamilyIndex = i;
            break;
        }
    }

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }
}

void CVulkanRenderer::CreateCommandBuffers()
{
    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

    if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void CVulkanRenderer::CreateSyncObjects()
{
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

// --------------------------------------------------------------------------------
// IRenderer Interface Implementation Stubs (unchanged or minimally adjusted)
// --------------------------------------------------------------------------------

bool CVulkanRenderer::SetCurrentContext(WIN_HWND hWnd) { return true; }
bool CVulkanRenderer::CreateContext(WIN_HWND hWnd, bool bAllowFSAA) { return true; }
bool CVulkanRenderer::DeleteContext(WIN_HWND hWnd) { return true; }

int CVulkanRenderer::CreateRenderTarget (int nWidth, int nHeight, ETEX_Format eTF) { return 0; }
bool CVulkanRenderer::DestroyRenderTarget (int nHandle) { return true; }
bool CVulkanRenderer::SetRenderTarget (int nHandle) { return true; }

int CVulkanRenderer::EnumDisplayFormats(TArray<SDispFormat>& Formats, bool bReset) { return 0; }
int CVulkanRenderer::EnumAAFormats(TArray<SAAFormat>& Formats, bool bReset) { return 0; }

bool CVulkanRenderer::ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen)
{
    // TODO: Recreate swapchain
    return true;
}

void CVulkanRenderer::Reset() {}

void *CVulkanRenderer::GetDynVBPtr(int nVerts, int &nOffs, int Pool) {
    int stride = 0;
    if (m_RP.m_CurVFormat > 0 && m_RP.m_CurVFormat < VERTEX_FORMAT_NUMS)
        stride = m_VertexSize[m_RP.m_CurVFormat];

    if (stride == 0) stride = sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F); // Fallback

    int size = nVerts * stride;

    // Align offset
    int alignedOffset = (m_DynVBOffset + stride - 1) / stride * stride;

    if (alignedOffset + size > m_DynVBSize) {
        return NULL;
    }

    nOffs = alignedOffset / stride;
    m_DynVBOffset = alignedOffset + size;

    return (byte*)m_DynVBMapped[m_CurrentFrame] + alignedOffset;
}

void CVulkanRenderer::DrawDynVB(int nOffs, int Pool, int nVerts) {
    if (m_CurrentFrame >= MAX_FRAMES_IN_FLIGHT) return;
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    VkBuffer vertexBuffers[] = { m_DynVBs[m_CurrentFrame].buffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

    vkCmdDraw(cmd, nVerts, 1, nOffs, 0);
}

void CVulkanRenderer::DrawDynVB(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pBuf, ushort *pInds, int nVerts, int nInds, int nPrimType) {
    // Simple implementation: Copy to DynVB and draw
    // Note: Does not handle indices yet!
    // For now, assume non-indexed if pInds is null, or ignore pInds

    if (!pBuf) return;

    // Save current format
    int oldFmt = m_RP.m_CurVFormat;
    m_RP.m_CurVFormat = VERTEX_FORMAT_P3F_COL4UB_TEX2F;

    int nOffs;
    void* ptr = GetDynVBPtr(nVerts, nOffs, 0);
    if (ptr)
    {
        memcpy(ptr, pBuf, nVerts * sizeof(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F));
        DrawDynVB(nOffs, 0, nVerts);
    }

    m_RP.m_CurVFormat = oldFmt;
}

uint32_t CVulkanRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void CVulkanRenderer::CreateVulkanBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);
}

CVertexBuffer *CVulkanRenderer::CreateBuffer(int vertexcount,int vertexformat, const char *szSource, bool bDynamic)
{
    CVertexBuffer * vtemp = new CVertexBuffer;
    vtemp->m_bDynamic = bDynamic;
    vtemp->m_vertexformat = vertexformat;
    vtemp->m_NumVerts = vertexcount;

    int size = m_VertexSize[vertexformat] * vertexcount;

    // Allocate Vulkan Buffer
    VulkanBuffer* vb = new VulkanBuffer;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    // Using Host Visible for now for simplicity
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    CreateVulkanBuffer(size, usage, properties, vb->buffer, vb->memory);
    vb->size = size;

    vtemp->m_VS[VSF_GENERAL].m_VertBuf.m_pPtr = vb;

    return vtemp;
}

void CVulkanRenderer::CreateBuffer(int size, int vertexformat, CVertexBuffer *buf, int Type, const char *szSource)
{
    // This overload seems to be used when appending to existing buffer or creating specific stream?
    // The OGL implementation creates a new buffer if supported.

    VulkanBuffer* vb = new VulkanBuffer;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    CreateVulkanBuffer(size, usage, properties, vb->buffer, vb->memory);
    vb->size = size;

    buf->m_VS[Type].m_VertBuf.m_pPtr = vb;
}

void CVulkanRenderer::ReleaseBuffer(CVertexBuffer *bufptr)
{
    if (bufptr)
    {
        for (int i=0; i<VSF_NUM; i++)
        {
            VulkanBuffer* vb = (VulkanBuffer*)bufptr->m_VS[i].m_VertBuf.m_pPtr;
            if (vb)
            {
                vkDestroyBuffer(m_Device, vb->buffer, nullptr);
                vkFreeMemory(m_Device, vb->memory, nullptr);
                delete vb;
                bufptr->m_VS[i].m_VertBuf.m_pPtr = NULL;
            }
        }
        delete bufptr;
    }
}

void CVulkanRenderer::DrawBuffer(CVertexBuffer *src,SVertexStream *indicies,int numindices, int offsindex, int prmode,int vert_start,int vert_stop, CMatInfo *mi)
{
    if (m_CurrentFrame >= MAX_FRAMES_IN_FLIGHT) return;
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    // Bind VB
    if (src)
    {
        VulkanBuffer* vb = (VulkanBuffer*)src->m_VS[VSF_GENERAL].m_VertBuf.m_pPtr;
        if (vb) {
            VkBuffer vertexBuffers[] = { vb->buffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        }
    }

    // Bind IB & Draw
    if (indicies && indicies->m_VertBuf.m_pPtr) {
        VulkanBuffer* ib = (VulkanBuffer*)indicies->m_VertBuf.m_pPtr;
        vkCmdBindIndexBuffer(cmd, ib->buffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, numindices, 1, offsindex, vert_start, 0);
    } else {
        // Non-indexed draw
        vkCmdDraw(cmd, numindices, 1, vert_start, 0);
    }
}

void CVulkanRenderer::UpdateBuffer(CVertexBuffer *dest,const void *src,int vertexcount, bool bUnLock, int offs, int Type)
{
    VulkanBuffer* vb = (VulkanBuffer*)dest->m_VS[Type].m_VertBuf.m_pPtr;
    if (!vb) return;

    if (src && vertexcount)
    {
        // Copy from src to buffer
        void* data;
        vkMapMemory(m_Device, vb->memory, 0, vb->size, 0, &data);

        int stride = 0;
        if (Type == VSF_GENERAL)
            stride = m_VertexSize[dest->m_vertexformat];
        else if (Type == VSF_TANGENTS)
            stride = sizeof(SPipTangents);

        if (stride > 0)
            memcpy((char*)data + offs * stride, src, vertexcount * stride);

        vkUnmapMemory(m_Device, vb->memory);
    }
    else
    {
        // Locking/Unlocking
        if (bUnLock)
        {
            if (dest->m_VS[Type].m_bLocked)
            {
                vkUnmapMemory(m_Device, vb->memory);
                dest->m_VS[Type].m_bLocked = false;
                dest->m_VS[Type].m_VData = NULL;
            }
        }
        else
        {
            if (!dest->m_VS[Type].m_bLocked)
            {
                void* data;
                vkMapMemory(m_Device, vb->memory, 0, vb->size, 0, &data);
                dest->m_VS[Type].m_VData = data;
                dest->m_VS[Type].m_bLocked = true;
            }
        }
    }
}

void CVulkanRenderer::CreateIndexBuffer(SVertexStream *dest,const void *src,int indexcount)
{
    ReleaseIndexBuffer(dest);
    if (indexcount)
    {
        VulkanBuffer* vb = new VulkanBuffer;
        VkDeviceSize size = indexcount * sizeof(ushort);

        CreateVulkanBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vb->buffer, vb->memory);
        vb->size = size;

        dest->m_VertBuf.m_pPtr = vb;
        dest->m_nItems = indexcount;
    }
    if (src && indexcount)
    {
        UpdateIndexBuffer(dest, src, indexcount, true);
    }
}

void CVulkanRenderer::UpdateIndexBuffer(SVertexStream *dest,const void *src,int indexcount, bool bUnLock)
{
    VulkanBuffer* vb = (VulkanBuffer*)dest->m_VertBuf.m_pPtr;
    if (!vb) return;

    if (src && indexcount)
    {
        void* data;
        vkMapMemory(m_Device, vb->memory, 0, vb->size, 0, &data);
        memcpy(data, src, indexcount * sizeof(ushort));
        vkUnmapMemory(m_Device, vb->memory);
    }
    else
    {
         if (bUnLock)
        {
            if (dest->m_bLocked)
            {
                vkUnmapMemory(m_Device, vb->memory);
                dest->m_bLocked = false;
                dest->m_VData = NULL;
            }
        }
        else
        {
            if (!dest->m_bLocked)
            {
                void* data;
                vkMapMemory(m_Device, vb->memory, 0, vb->size, 0, &data);
                dest->m_VData = data;
                dest->m_bLocked = true;
            }
        }
    }
}

void CVulkanRenderer::ReleaseIndexBuffer(SVertexStream *dest)
{
    VulkanBuffer* vb = (VulkanBuffer*)dest->m_VertBuf.m_pPtr;
    if (vb)
    {
        vkDestroyBuffer(m_Device, vb->buffer, nullptr);
        vkFreeMemory(m_Device, vb->memory, nullptr);
        delete vb;
        dest->m_VertBuf.m_pPtr = NULL;
    }
    dest->Reset();
}

void CVulkanRenderer::CheckError(const char *comment) {}

void CVulkanRenderer::Draw3dBBox(const Vec3 &mins, const Vec3 &maxs, int nPrimType)
{
    VulkanPipelineState state;
    state.renderPass = m_RenderPass;
    state.vertexShader = m_VS2D;
    state.fragmentShader = m_PS2D;
    state.vertexFormat = VERTEX_FORMAT_P3F_COL4UB_TEX2F;
    state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; // Debug boxes are lines

    CVulkanPipeline* pPipeline = GetPipeline(state);
    if (!pPipeline) return;

    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pPipeline->GetPipeline());

    Matrix44 MVP = m_ProjMatrix * m_ViewMatrix;
    vkCmdPushConstants(cmd, pPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Matrix44), &MVP);

    // Get Dynamic Buffer
    int nOffs;
    int oldFmt = m_RP.m_CurVFormat;
    m_RP.m_CurVFormat = VERTEX_FORMAT_P3F_COL4UB_TEX2F;
    struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F* pV = (struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F*)GetDynVBPtr(24, nOffs, 0);
    if (!pV) {
        m_RP.m_CurVFormat = oldFmt;
        return;
    }

    DWORD col = 0xFFFFFFFF; // White

    Vec3 p[8];
    p[0] = Vec3(mins.x, mins.y, mins.z);
    p[1] = Vec3(maxs.x, mins.y, mins.z);
    p[2] = Vec3(maxs.x, maxs.y, mins.z);
    p[3] = Vec3(mins.x, maxs.y, mins.z);
    p[4] = Vec3(mins.x, mins.y, maxs.z);
    p[5] = Vec3(maxs.x, mins.y, maxs.z);
    p[6] = Vec3(maxs.x, maxs.y, maxs.z);
    p[7] = Vec3(mins.x, maxs.y, maxs.z);

    // Bottom
    pV[0].xyz = p[0]; pV[0].color.dcolor = col;
    pV[1].xyz = p[1]; pV[1].color.dcolor = col;
    pV[2].xyz = p[1]; pV[2].color.dcolor = col;
    pV[3].xyz = p[2]; pV[3].color.dcolor = col;
    pV[4].xyz = p[2]; pV[4].color.dcolor = col;
    pV[5].xyz = p[3]; pV[5].color.dcolor = col;
    pV[6].xyz = p[3]; pV[6].color.dcolor = col;
    pV[7].xyz = p[0]; pV[7].color.dcolor = col;

    // Top
    pV[8].xyz = p[4]; pV[8].color.dcolor = col;
    pV[9].xyz = p[5]; pV[9].color.dcolor = col;
    pV[10].xyz = p[5]; pV[10].color.dcolor = col;
    pV[11].xyz = p[6]; pV[11].color.dcolor = col;
    pV[12].xyz = p[6]; pV[12].color.dcolor = col;
    pV[13].xyz = p[7]; pV[13].color.dcolor = col;
    pV[14].xyz = p[7]; pV[14].color.dcolor = col;
    pV[15].xyz = p[4]; pV[15].color.dcolor = col;

    // Sides
    pV[16].xyz = p[0]; pV[16].color.dcolor = col;
    pV[17].xyz = p[4]; pV[17].color.dcolor = col;
    pV[18].xyz = p[1]; pV[18].color.dcolor = col;
    pV[19].xyz = p[5]; pV[19].color.dcolor = col;
    pV[20].xyz = p[2]; pV[20].color.dcolor = col;
    pV[21].xyz = p[6]; pV[21].color.dcolor = col;
    pV[22].xyz = p[3]; pV[22].color.dcolor = col;
    pV[23].xyz = p[7]; pV[23].color.dcolor = col;

    DrawDynVB(nOffs, 0, 24);

    m_RP.m_CurVFormat = oldFmt;
}
void CVulkanRenderer::SetCamera(const CCamera &cam) {}
void CVulkanRenderer::SetViewport(int x, int y, int width, int height) {}
void CVulkanRenderer::SetScissor(int x, int y, int width, int height) {}

void CVulkanRenderer::SetCullMode(int mode) {}
bool CVulkanRenderer::EnableFog(bool enable) { return true; }
void CVulkanRenderer::SetFog(float density,float fogstart,float fogend,const float *color,int fogmode) {}
void CVulkanRenderer::SetFogColor(float * color) {}

void CVulkanRenderer::EnableTexGen(bool enable) {}
void CVulkanRenderer::SetTexgen(float scaleX,float scaleY,float translateX,float translateY) {}
void CVulkanRenderer::SetTexgen3D(float x1, float y1, float z1, float x2, float y2, float z2) {}
void CVulkanRenderer::SetLodBias(float value) {}
void CVulkanRenderer::EnableVSync(bool enable) {}

// Matrix operations
void CVulkanRenderer::PushMatrix()
{
    m_ViewMatrixStack.push_back(m_ViewMatrix);
}

void CVulkanRenderer::PopMatrix()
{
    if (!m_ViewMatrixStack.empty())
    {
        m_ViewMatrix = m_ViewMatrixStack.back();
        m_ViewMatrixStack.pop_back();
    }
}

void CVulkanRenderer::RotateMatrix(float a,float x,float y,float z)
{
    // TODO: Apply rotation to m_ViewMatrix
    // For now simple implementation using Cry_Matrix functions if available or manual
    // Matrix44 rot = Matrix44::CreateRotationAA(DEG2RAD(a), Vec3(x,y,z));
    // m_ViewMatrix = m_ViewMatrix * rot;
}

void CVulkanRenderer::RotateMatrix(const Vec3 & angels)
{
    // TODO
}

void CVulkanRenderer::TranslateMatrix(float x,float y,float z)
{
    // TODO
}

void CVulkanRenderer::ScaleMatrix(float x,float y,float z)
{
    // TODO
}

void CVulkanRenderer::TranslateMatrix(const Vec3 &pos)
{
    // TODO
}

void CVulkanRenderer::MultMatrix(float * mat)
{
    // TODO
}

void CVulkanRenderer::LoadMatrix(const Matrix44 *src)
{
    if (src)
        m_ViewMatrix = *src;
    else
        m_ViewMatrix.SetIdentity();
}

static void MakeOrthoMatrix(Matrix44& m, float l, float r, float b, float t, float zn, float zf)
{
    // Standard Ortho:
    // 2/(r-l)      0            0           -(r+l)/(r-l)
    // 0            2/(t-b)      0           -(t+b)/(t-b)
    // 0            0            1/(zf-zn)   -zn/(zf-zn)   <-- Vulkan 0..1 Z
    // 0            0            0           1

    // Vulkan NDC Y is down (top -1, bottom 1)
    // Screen coords: Top (0), Bottom (h)
    // We want 0 -> -1 (Top), h -> 1 (Bottom).
    // y' = (y - 0) / (h - 0) * 2 - 1 = 2y/h - 1.
    // 2/h * y - 1.
    // So M[1][1] = 2/h. M[1][3] = -1.

    // X: 0 -> -1, w -> 1.
    // x' = 2x/w - 1.
    // M[0][0] = 2/w. M[0][3] = -1.

    // BUT Matrix44 is Row-Major? M03 is translation X.
    // So M03 = -1. M00 = 2/w.

    // In D3D Set2DMode: l=0, r=w, t=0, b=h.

    float w = r - l;
    float h = b - t; // if b=h, t=0

    m.SetIdentity();

    m.M00 = 2.0f / w;
    m.M03 = -1.0f; // (l+r)/(l-r) ? if l=0, r=w, then w/-w = -1. Correct.

    // Y
    // D3D (Y-up NDC): maps 0(t) to 1, h(b) to -1.
    // Vulkan (Y-down NDC): maps 0(t) to -1, h(b) to 1.
    // t=0, b=h.
    // 2/(b-t) = 2/h.
    // -(b+t)/(b-t) = -h/h = -1.
    m.M11 = 2.0f / h;
    m.M13 = -1.0f;

    // Z (0..1)
    m.M22 = 1.0f / (zf - zn);
    m.M23 = -zn / (zf - zn);

    m.M33 = 1.0f;

    // Note: This assumes input coordinates are 0..w, 0..h
}

void CVulkanRenderer::Set2DMode(bool enable, int ortox, int ortoy)
{
    if (enable)
    {
        m_ProjMatrixStack.push_back(m_ProjMatrix);

        MakeOrthoMatrix(m_ProjMatrix, 0.0f, (float)ortox, (float)ortoy, 0.0f, -1e30f, 1e30f);

        m_ViewMatrixStack.push_back(m_ViewMatrix);
        m_ViewMatrix.SetIdentity();
    }
    else
    {
        if (!m_ProjMatrixStack.empty())
        {
            m_ProjMatrix = m_ProjMatrixStack.back();
            m_ProjMatrixStack.pop_back();
        }
        if (!m_ViewMatrixStack.empty())
        {
            m_ViewMatrix = m_ViewMatrixStack.back();
            m_ViewMatrixStack.pop_back();
        }
    }
}

void CVulkanRenderer::DrawTriStrip(CVertexBuffer *src, int vert_num) {
    if (!src || !vert_num) return;

    // Create a temporary buffer for immediate mode drawing
    // Note: In a real implementation, we should use a dynamic ring buffer to avoid frequent allocations.
    CVertexBuffer* vb = CreateBuffer(vert_num, src->m_vertexformat, "DrawTriStrip_Temp", true);

    // Copy data from source to the new buffer
    // Assuming src->m_VS[VSF_GENERAL].m_VData points to the data in system memory
    if (src->m_VS[VSF_GENERAL].m_VData) {
        UpdateBuffer(vb, src->m_VS[VSF_GENERAL].m_VData, vert_num, true);
    }

    // Draw using DrawBuffer
    DrawBuffer(vb, NULL, vert_num, 0, R_PRIMV_TRIANGLE_STRIP, 0, vert_num);

    // Release the temporary buffer
    ReleaseBuffer(vb);
}


void CVulkanRenderer::EnableTMU(bool enable) {}
void CVulkanRenderer::SelectTMU(int tnum) {}

bool CVulkanRenderer::ChangeDisplay(unsigned int width,unsigned int height,unsigned int cbpp) { return true; }
void CVulkanRenderer::ChangeViewport(unsigned int x,unsigned int y,unsigned int width,unsigned int height) {}

unsigned int CVulkanRenderer::DownLoadToVideoMemory(unsigned char *data,int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat, int filter, int Id, char *szCacheName, int flags)
{
    // This is used for procedural textures or lightmaps etc.
    // Use TexMan to create a texture
    char name[128];
    sprintf(name, "$Auto_%d", m_TexGenID++);
    int creationFlags = nummipmap ? 0 : FT_NOMIPS | FT_NOWORLD;
    int DXTSize = 0;

    if (eTFSrc == eTF_DXT1) creationFlags |= FT_DXT1;
    if (eTFSrc == eTF_DXT3) creationFlags |= FT_DXT3;
    if (eTFSrc == eTF_DXT5) creationFlags |= FT_DXT5;

    if (!repeat) creationFlags |= FT_CLAMP;
    if (eTFDst == eTF_8888) creationFlags |= FT_HASALPHA;

    STexPic *tp = m_TexMan->CreateTexture(name, w, h, 1, creationFlags, FT2_NODXT, data, eTT_Base, -1.0f, -1.0f, DXTSize, NULL, Id, eTFSrc);
    return tp->m_Bind;
}

void CVulkanRenderer::UpdateTextureInVideoMemory(uint tnum, unsigned char *newdata,int posx,int posy,int w,int h,ETEX_Format eTF)
{
    STexPic* pTex = m_TexMan->GetByID(tnum);
    if (pTex)
    {
        m_TexMan->UpdateTextureRegion(pTex, newdata, posx, posy, w, h);
    }
}

unsigned int CVulkanRenderer::LoadTexture(const char * filename,int *tex_type,unsigned int def_tid,bool compresstodisk,bool bWarn)
{
    if (def_tid == 0) def_tid = -1;
    ITexPic * pPic = EF_LoadTexture(filename, FT_NOREMOVE, 0, eTT_Base, -1, -1, def_tid);
    if (pPic && pPic->IsTextureLoaded())
        return pPic->GetTextureID();
    return 0;
}

bool CVulkanRenderer::SetGammaDelta(const float fGamma) { return true; }

void CVulkanRenderer::RemoveTexture(unsigned int TextureId)
{
    if (TextureId)
    {
        STexPic *tp = m_TexMan->GetByID(TextureId);
        if (tp)
            tp->Release(false);
    }
}

void CVulkanRenderer::RemoveTexture(ITexPic * pTexPic)
{
    if (pTexPic)
    {
        STexPic * pSTexPic = (STexPic *)pTexPic;
        pSTexPic->Release(false);
    }
}

void CVulkanRenderer::SetTexture(int tnum, ETexType Type)
{
    m_TexMan->SetTexture(tnum, Type);
}

void CVulkanRenderer::SetWhiteTexture()
{
    m_TexMan->m_Text_White->Set();
}

void CVulkanRenderer::Draw2dImage(float xpos,float ypos,float w,float h,int texture_id,float s0,float t0,float s1,float t1,float angle,float r,float g,float b,float a,float z)
{
    if (m_CurrentFrame >= MAX_FRAMES_IN_FLIGHT) return;

    Set2DMode(true, m_width, m_height);

    // Create Vertices
    struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F verts[4];
    DWORD col = (DWORD(a*255.0f)<<24) | (DWORD(r*255.0f)<<16) | (DWORD(g*255.0f)<<8) | DWORD(b*255.0f);

    float fx = xpos; // Top-Left X
    float fy = ypos; // Top-Left Y
    float fw = w;
    float fh = h;

    // TODO: Handle angle rotation (simplified for now)

    verts[0].xyz.x = fx;      verts[0].xyz.y = fy;      verts[0].xyz.z = z;
    verts[0].color.dcolor = col;
    verts[0].st[0] = s0;      verts[0].st[1] = 1.0f-t0; // D3D9 texture coords might need flip, keeping D3D logic

    verts[1].xyz.x = fx + fw; verts[1].xyz.y = fy;      verts[1].xyz.z = z;
    verts[1].color.dcolor = col;
    verts[1].st[0] = s1;      verts[1].st[1] = 1.0f-t0;

    verts[2].xyz.x = fx + fw; verts[2].xyz.y = fy + fh; verts[2].xyz.z = z;
    verts[2].color.dcolor = col;
    verts[2].st[0] = s1;      verts[2].st[1] = 1.0f-t1;

    verts[3].xyz.x = fx;      verts[3].xyz.y = fy + fh; verts[3].xyz.z = z;
    verts[3].color.dcolor = col;
    verts[3].st[0] = s0;      verts[3].st[1] = 1.0f-t1;

    // Create Temporary Buffer
    CVertexBuffer* vb = CreateBuffer(4, VERTEX_FORMAT_P3F_COL4UB_TEX2F, "Draw2dImage", true);
    UpdateBuffer(vb, verts, 4, true);

    // Setup Pipeline
    VulkanPipelineState state;
    state.renderPass = m_RenderPass;
    state.vertexShader = m_VS2D;
    state.fragmentShader = m_PS2D;
    state.vertexFormat = VERTEX_FORMAT_P3F_COL4UB_TEX2F;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; // Or List with index buffer
    // Note: Vulkan doesn't support Triangle Fan natively in core without extensions often, use Triangle List with Index Buffer
    // For now, let's assume we can use Triangle List if we create Index Buffer or just 2 triangles (6 verts)
    // Changing to 4 verts Triangle Strip for simplicity?
    // D3D9 implementation uses Triangle Fan.
    // Let's use Triangle Strip with 4 verts. Order: 0, 1, 3, 2 (TopLeft, TopRight, BotLeft, BotRight)

    // Reordering for Strip:
    // 0: TL (x, y)
    // 1: TR (x+w, y)
    // 2: BL (x, y+h)
    // 3: BR (x+w, y+h)

    struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F vertsStrip[4];
    vertsStrip[0] = verts[0]; // TL
    vertsStrip[1] = verts[1]; // TR
    vertsStrip[2] = verts[3]; // BL
    vertsStrip[3] = verts[2]; // BR

    UpdateBuffer(vb, vertsStrip, 4, true);
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    CVulkanPipeline* pPipeline = GetPipeline(state);
    if (pPipeline)
    {
        VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pPipeline->GetPipeline());

        // Push Constants
        // Use m_ProjMatrix. View and Model are Identity in 2D Mode usually
        vkCmdPushConstants(cmd, pPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Matrix44), &m_ProjMatrix);

        // TODO: Bind Texture (Descriptor Sets)

        VulkanBuffer* pVB = (VulkanBuffer*)vb->m_VS[VSF_GENERAL].m_VertBuf.m_pPtr;
        VkBuffer vertexBuffers[] = { pVB->buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

        vkCmdDraw(cmd, 4, 1, 0, 0);
    }

    ReleaseBuffer(vb);
    Set2DMode(false, m_width, m_height);
}

int CVulkanRenderer::SetPolygonMode(int mode) { return 0; }
void CVulkanRenderer::ResetToDefault() {}
int CVulkanRenderer::GenerateAlphaGlowTexture(float k) { return 0; }
void CVulkanRenderer::SetMaterialColor(float r, float g, float b, float a) {}
int CVulkanRenderer::LoadAnimatedTexture(const char * format,const int nCount) { return 0; }
char * CVulkanRenderer::GetStatusText(ERendStats type) { return ""; }
void CVulkanRenderer::GetMemoryUsage(ICrySizer* Sizer) {}

void CVulkanRenderer::ProjectToScreen( float ptx, float pty, float ptz, float *sx, float *sy, float *sz ) {}
int CVulkanRenderer::UnProject(float sx, float sy, float sz, float *px, float *py, float *pz, const float modelMatrix[16], const float projMatrix[16], const int viewport[4]) { return 0; }
int CVulkanRenderer::UnProjectFromScreen( float  sx, float  sy, float  sz, float *px, float *py, float *pz) { return 0; }
void CVulkanRenderer::Draw2dLine(float x1,float y1,float x2,float y2) {}
void CVulkanRenderer::DrawLine(const Vec3 & vPos1, const Vec3 & vPos2) {}
void CVulkanRenderer::DrawLineColor(const Vec3 & vPos1, const CFColor & vColor1, const Vec3 & vPos2, const CFColor & vColor2) {}
void CVulkanRenderer::DrawBall(float x, float y, float z, float radius) {}
void CVulkanRenderer::DrawBall(const Vec3 & pos, float radius ) {}
void CVulkanRenderer::DrawPoint(float x, float y, float z, float fSize) {}

// Shadows
void CVulkanRenderer::PrepareDepthMap(ShadowMapFrustum * lof, bool make_new_tid) {}
void CVulkanRenderer::SetupShadowOnlyPass(int Num, ShadowMapFrustum * pFrustum, Vec3 * vShadowTrans, const float fShadowScale, Vec3 vObjTrans, float fObjScale, const Vec3 vObjAngles, Matrix44 * pObjMat) {}
void CVulkanRenderer::DrawAllShadowsOnTheScreen() {}
void CVulkanRenderer::OnEntityDeleted(IEntityRender * pEntityRender) {}

void CVulkanRenderer::SetClipPlane( int id, float * params ) {}
void CVulkanRenderer::EF_SetClipPlane (bool bEnable, float *pPlane, bool bRefract) {}
void CVulkanRenderer::SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa) {}

void CVulkanRenderer::GetModelViewMatrix(float *mat) {}
void CVulkanRenderer::GetProjectionMatrix(float *mat) {}

void CVulkanRenderer::DrawObjSprites(list2<CStatObjInst*> *pList, float fMaxViewDist, CObjManager *pObjMan) {}
void CVulkanRenderer::DrawQuad(const Vec3 &right, const Vec3 &up, const Vec3 &origin,int nFlipMode) {}
void CVulkanRenderer::DrawQuad(float dy,float dx, float dz, float x, float y, float z) {}

void CVulkanRenderer::ClearDepthBuffer() {}
void CVulkanRenderer::ClearColorBuffer(const Vec3 vColor) {}
void CVulkanRenderer::ReadFrameBuffer(unsigned char * pRGB, int nSizeX, int nSizeY, bool bBackBuffer, bool bRGBA, int nScaledX, int nScaledY) {}

void CVulkanRenderer::TransformTextureMatrix(float x, float y, float angle, float scale) {}
void CVulkanRenderer::ResetTextureMatrix() {}
void CVulkanRenderer::ScreenShot(const char *filename) {}

char* CVulkanRenderer::GetVertexProfile(bool bSupportedProfile) { return ""; }
char* CVulkanRenderer::GetPixelProfile(bool bSupportedProfile) { return ""; }
unsigned int CVulkanRenderer::MakeSprite(float object_scale, int tex_size, float angle, IStatObj * pStatObj, uchar * pTmpBuffer, uint def_tid) { return 0; }
unsigned int CVulkanRenderer::Make3DSprite(int nTexSize, float fAngleStep, IStatObj * pStatObj) { return 0; }

void CVulkanRenderer::Set2DMode(bool enable, int ortox, int ortoy) {}
int CVulkanRenderer::ScreenToTexture() { return 0; }
void CVulkanRenderer::SetTexClampMode(bool clamp) {}

void CVulkanRenderer::DrawPoints(Vec3 v[], int nump, CFColor& col, int flags) {}
void CVulkanRenderer::DrawLines(Vec3 v[], int nump, CFColor& col, int flags, float fGround) {}

// Shaders / Effects
void CVulkanRenderer::EF_Release(int nFlags) {}
void CVulkanRenderer::EF_PipelineShutdown() {}
void CVulkanRenderer::EF_LightMaterial(SLightMaterial *lm, int Flags) {}
void CVulkanRenderer::EF_CheckOverflow(int nVerts, int nTris, CRendElement *re) {}
void CVulkanRenderer::EF_Start(SShader *ef, SShader *efState, SRenderShaderResources *Res, int nFog, CRendElement *re)
{
    m_RP.m_RendPass = 0;
    m_RP.m_FirstIndex = 0;
    m_RP.m_FirstVertex = 0;
    m_RP.m_BaseVertex = 0;
    m_RP.m_RendNumIndices = 0;
    m_RP.m_RendNumVerts = 0;
    m_RP.m_pShader = ef;
    m_RP.m_pStateShader = efState;
    m_RP.m_pShaderResources = Res;
    m_RP.m_pCurEnvTexture = NULL;
#ifdef PIPE_USE_INSTANCING
    m_RP.m_MergedObjects.SetUse(0);
#endif
    m_RP.m_pCurLightMaterial = NULL;
    m_RP.m_FlagsPerFlush = 0;
    m_RP.m_FlagsModificators = 0;
    m_RP.m_fCurOpacity = 1.0f;
    if (nFog && CV_r_VolumetricFog)
        m_RP.m_pFogVolume = &m_RP.m_FogVolumes[nFog];
    else
        m_RP.m_pFogVolume = NULL;

    if (m_RP.m_pCurObject)
        m_RP.m_ObjFlags = m_RP.m_pCurObject->m_ObjFlags;
    else
        m_RP.m_ObjFlags = 0;

    m_RP.m_CurVFormat = ef->m_VertexFormatId;

    SBufInfoTable *pOffs = &gBufInfoTable[m_RP.m_CurVFormat];
    int Size = m_VertexSize[m_RP.m_CurVFormat];
    m_RP.m_Stride = Size;
    m_RP.m_OffsD  = pOffs->OffsColor;
    m_RP.m_OffsT  = pOffs->OffsTC;
    m_RP.m_OffsN  = pOffs->OffsNormal;
    m_RP.m_NextPtr = m_RP.m_Ptr;

    if (m_RP.m_pCurObject)
        m_RP.m_DynLMask = m_RP.m_pCurObject->m_DynLMMask;
    else
        m_RP.m_DynLMask = 0;

    m_RP.m_MergedREs.SetUse(0);
    m_RP.m_MergedObjs.SetUse(0);

    if (!EF_BuildLightsList())
    {
        if (m_pLog) m_pLog->Log("WARNING: CVulkanRenderer::EF_BuildLightsList: Too many light sources per render item (> 16). Shader: '%s'\n", ef->m_Name.c_str());
    }

    // Choose appropriate shader technique depend on some input parameters
    if (ef->m_HWTechniques.Num())
    {
        m_RP.m_pRE = re;
        int nHW = EF_SelectHWTechnique(ef);
        if (nHW >= 0)
            m_RP.m_pCurTechnique = ef->m_HWTechniques[nHW];
        else
            m_RP.m_pCurTechnique = NULL;
    }
    else
        m_RP.m_pCurTechnique = NULL;

    m_RP.m_pRE = NULL;

    m_RP.m_Frame++;
}

void CVulkanRenderer::EF_Start(SShader *ef, SShader *efState, SRenderShaderResources *Res, CRendElement *re)
{
    m_RP.m_RendPass = 0;
    m_RP.m_RendNumIndices = 0;
    m_RP.m_RendNumVerts = 0;
    m_RP.m_FirstIndex = 0;
    m_RP.m_FirstVertex = 0;
    m_RP.m_BaseVertex = 0;
    m_RP.m_pShader = ef;
#ifdef PIPE_USE_INSTANCING
    m_RP.m_MergedObjects.SetUse(0);
#endif
    m_RP.m_pCurLightMaterial = NULL;
    m_RP.m_pStateShader = efState;
    m_RP.m_pShaderResources = Res;
    m_RP.m_FlagsPerFlush = 0;
    m_RP.m_FlagsModificators = 0;
    m_RP.m_pFogVolume = NULL;
    m_RP.m_pRE = NULL;
    m_RP.m_fCurOpacity = 1.0f;

    // Choose appropriate shader technique depend on some input parameters
    if (ef->m_HWTechniques.Num())
    {
        int nHW = EF_SelectHWTechnique(ef);
        if (nHW >= 0)
            m_RP.m_pCurTechnique = ef->m_HWTechniques[nHW];
        else
            m_RP.m_pCurTechnique = NULL;
    }
    else
        m_RP.m_pCurTechnique = NULL;

    m_RP.m_Frame++;
}
bool CVulkanRenderer::EF_SetLightHole(Vec3 vPos, Vec3 vNormal, int idTex, float fScale, bool bAdditive) { return true; }
STexPic *CVulkanRenderer::EF_MakePhongTexture(int Exp) { return NULL; }

void CVulkanRenderer::EF_EndEf3D (int nFlags)
{
    if (m_bDeviceLost)
    {
        SRendItem::m_RecurseLevel--;
        return;
    }

    assert(SRendItem::m_RecurseLevel >= 1);
    if (SRendItem::m_RecurseLevel < 1)
    {
        if (m_pLog) m_pLog->Log("Error: CVulkanRenderer::EF_EndEf3D without CVulkanRenderer::EF_StartEf");
        return;
    }

    if (CV_r_nodrawshaders == 1)
    {
        SetClearColor(Vec3d(0,0,0));
        // EF_ClearBuffers(false, false, NULL); // TODO
        SRendItem::m_RecurseLevel--;
        return;
    }

    m_RP.m_PersFlags &= ~(RBPF_DRAWNIGHTMAP | RBPF_DRAWHEATMAP);
    m_RP.m_RealTime = iTimer->GetCurrTime();

    if (CV_r_fullbrightness)
    {
        m_RP.m_NeedGlobalColor.dcolor = -1;
        m_RP.m_FlagsPerFlush |= RBSI_RGBGEN | RBSI_ALPHAGEN;
    }

    if (CV_r_excludeshader && CV_r_excludeshader->GetString()[0] != '0')
        m_RP.m_ExcludeShader = CV_r_excludeshader->GetString();
    else
        m_RP.m_ExcludeShader = NULL;

    if (CV_r_showonlyshader && CV_r_showonlyshader->GetString()[0] != '0')
        m_RP.m_ShowOnlyShader = CV_r_showonlyshader->GetString();
    else
        m_RP.m_ShowOnlyShader = NULL;

    EF_UpdateSplashes(m_RP.m_RealTime);
    EF_AddClientPolys3D();
    EF_AddClientPolys2D();

    SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_PREPROCESS_ID] = SRendItem::m_RendItems[EFSLIST_PREPROCESS_ID].Num();
    SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_STENCIL_ID] = SRendItem::m_RendItems[EFSLIST_STENCIL_ID].Num();
    SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_GENERAL_ID] = SRendItem::m_RendItems[EFSLIST_GENERAL_ID].Num();
    SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_UNSORTED_ID] = SRendItem::m_RendItems[EFSLIST_UNSORTED_ID].Num();
    SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_DISTSORT_ID] = SRendItem::m_RendItems[EFSLIST_DISTSORT_ID].Num();
    SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_LAST_ID] = SRendItem::m_RendItems[EFSLIST_LAST_ID].Num();

    EF_RenderPipeLine(EF_Flush);

    EF_DrawDebugTools();
    EF_RemovePolysFromScene();
    SRendItem::m_RecurseLevel--;
}

void CVulkanRenderer::EF_EndEf2D(bool bSort) {}
int CVulkanRenderer::EF_RegisterFogVolume(float fMaxFogDist, float fFogLayerZ, CFColor color, int nIndex, bool bCaustics) { return 0; }

void CVulkanRenderer::EF_RenderPipeLine(void (*RenderFunc)())
{
    EF_PipeLine(SRendItem::m_StartRI[SRendItem::m_RecurseLevel-1][EFSLIST_PREPROCESS_ID], SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_PREPROCESS_ID], EFSLIST_PREPROCESS_ID, RenderFunc);  // Preprocess and probably sky

    if (!(m_RP.m_PersFlags & RBPF_IGNORERENDERING))
    {
        EF_PipeLine(SRendItem::m_StartRI[SRendItem::m_RecurseLevel-1][EFSLIST_STENCIL_ID], SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_STENCIL_ID], EFSLIST_STENCIL_ID, RenderFunc);   // Unsorted list for indoor
        EF_PipeLine(SRendItem::m_StartRI[SRendItem::m_RecurseLevel-1][EFSLIST_GENERAL_ID], SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_GENERAL_ID], EFSLIST_GENERAL_ID, RenderFunc);    // Sorted list without preprocess
        EF_PipeLine(SRendItem::m_StartRI[SRendItem::m_RecurseLevel-1][EFSLIST_UNSORTED_ID], SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_UNSORTED_ID], EFSLIST_UNSORTED_ID, RenderFunc); // Unsorted list
        EF_PipeLine(SRendItem::m_StartRI[SRendItem::m_RecurseLevel-1][EFSLIST_DISTSORT_ID], SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_DISTSORT_ID], EFSLIST_DISTSORT_ID, RenderFunc);   // Sorted by distance elements
        if (SRendItem::m_RecurseLevel <= 1)
            EF_PipeLine(SRendItem::m_StartRI[SRendItem::m_RecurseLevel-1][EFSLIST_LAST_ID], SRendItem::m_EndRI[SRendItem::m_RecurseLevel-1][EFSLIST_LAST_ID], EFSLIST_LAST_ID, RenderFunc);       // Sorted list without preprocess of all fog passes and screen shaders
    }
    else
        m_RP.m_PersFlags &= ~RBPF_IGNORERENDERING;
}

void CVulkanRenderer::EF_PipeLine(int nums, int nume, int nList, void (*RenderFunc)())
{
    int i;
    SShader *pShader, *pCurShader, *pShaderState, *pCurShaderState;
    SRenderShaderResources *pRes, *pCurRes;
    int nObject, nCurObject;
    int nFog, nCurFog;

    if (nume-nums < 1)
        return;

    // CheckDeviceLost();

    m_RP.m_pRenderFunc = RenderFunc;
    m_RP.m_nCurLightParam = -1;
    m_RP.m_pCurObject = m_RP.m_VisObjects[0];
    m_RP.m_pPrevObject = m_RP.m_pCurObject;

    EF_PreRender(1);

    if (nList==EFSLIST_PREPROCESS_ID || nList==EFSLIST_GENERAL_ID || nList==EFSLIST_LAST_ID)
    {
        SRendItem::mfSort(&SRendItem::m_RendItems[nList][nums], nume-nums);

        if ((SRendItem::m_RendItems[nList][nums].SortVal.i.High >> 26) == eS_PreProcess)
            nums += EF_Preprocess(&SRendItem::m_RendItems[nList][0], nums, nume);

        if (m_RP.m_PersFlags & RBPF_IGNORERENDERING)
            return;
    }
    else if (nList==EFSLIST_DISTSORT_ID)
    {
        SRendItem::mfSortByDist(&SRendItem::m_RendItems[nList][nums], nume-nums);
    }
    // TODO: STENCIL_ID sorting

    m_RP.m_Flags |= RBF_3D;

    EF_PreRender(3);
    EF_PushMatrix();

    UnINT64 oldVal;
    oldVal.SortVal = -1;
    nCurObject = -2;
    nCurFog = 0;
    pCurShader = NULL;
    pCurShaderState = NULL;
    pCurRes = NULL;
    bool bIgnore = false;
    bool bChanged;
    bool bUseBatching = (RenderFunc == EF_Flush);

    for (i=nums; i<nume; i++)
    {
        SRendItemPre *ri = &SRendItem::m_RendItems[nList][i];
        CRendElement *pRE = ri->Item;

#ifdef PIPE_USE_INSTANCING
        if (oldVal.i.High == ri->SortVal.i.High && !((oldVal.i.Low ^ ri->SortVal.i.Low) & 0x000fffff))
        {
            SRendItem::mfGetObj(ri->SortVal, &nObject);
            bChanged = false;
        }
        else
        {
            SRendItem::mfGet(ri->SortVal, &nObject, &pShader, &pShaderState, &nFog, &pRes);
            bChanged = true;
        }
        oldVal.SortVal = ri->SortVal.SortVal;
#else
        if (ri->SortVal.SortVal == oldVal.SortVal)
        {
            if (bIgnore) continue;
            pRE->mfPrepare();
            continue;
        }
        oldVal.SortVal = ri->SortVal.SortVal;
        SRendItem::mfGet(ri->SortVal, &nObject, &pShader, &pShaderState, &nFog, &pRes);
        bChanged = (pCurRes != pRes || pShader != pCurShader || pShaderState != pCurShaderState || nFog != nCurFog);
#endif

        if (nObject != nCurObject)
        {
            if (!bChanged && !pShader->m_Deforms && bUseBatching)
            {
                if (EF_TryToMerge(nObject, nCurObject, pRE))
                    continue;
            }
            if (pCurShader)
            {
                m_RP.m_pRenderFunc();
                pCurShader = NULL;
                bChanged = true;
            }
            if (!EF_ObjectChange(pShader, pRes, nObject, pRE))
            {
                bIgnore = true;
                continue;
            }
            bIgnore = false;
            nCurObject = nObject;
        }

        if (bChanged)
        {
            if (pCurShader)
                m_RP.m_pRenderFunc();
            EF_Start(pShader, pShaderState, pRes, nFog, pRE);
            nCurFog = nFog;
            pCurShader = pShader;
            pCurShaderState = pShaderState;
            pCurRes = pRes;
        }

        pRE->mfPrepare();
    }
    if (pCurShader)
        m_RP.m_pRenderFunc();

    EF_PostRender();
    EF_PopMatrix();
}

void CVulkanRenderer::EF_PreRender(int Stage)
{
    if (m_bDeviceLost)
        return;

    if (Stage & 1)
    { // Before preprocess
        m_RP.m_RealTime = iTimer->GetCurrTime();
        m_RP.m_Flags = 0;
        m_RP.m_pPrevObject = NULL;
        m_RP.m_FrameObject++;
    }

    if (Stage & 2)
    {  // After preprocess
        if (!m_RP.m_bStartPipeline && !m_bWasCleared && !(m_RP.m_PersFlags & RBPF_NOCLEARBUF))
        {
            m_RP.m_bStartPipeline = true;
            // EF_ClearBuffers(false, false, NULL); // TODO
        }
    }
    m_RP.m_pCurLight = NULL;
}

void CVulkanRenderer::EF_PostRender()
{
    EF_ObjectChange(NULL, NULL, 0, NULL);
    m_RP.m_pRE = NULL;

    m_RP.m_FlagsModificators = 0;
    m_RP.m_CurrentVLights = 0;
    m_RP.m_FlagsPerFlush = 0;

    m_RP.m_pShader = NULL;
    m_RP.m_pCurObject = m_RP.m_VisObjects[0];
}

void CVulkanRenderer::EF_Flush()
{
    gcpVulkan->EF_FlushShader();
}

void CVulkanRenderer::EF_FlushShader()
{
    SShader *ef = m_RP.m_pShader;
    if (!ef) return;

    // if (m_RP.m_pRE) { EF_InitEvalFuncs(1); } else { EF_InitEvalFuncs(0); if (!m_RP.m_RendNumIndices) return; }

    m_RP.m_ResourceState = 0;
    // EF_SetResourcesState(true);

    if (ef->m_HWTechniques.Num())
    {
        EF_FlushHW();
        // EF_SetResourcesState(false);
        return;
    }
}

void CVulkanRenderer::EF_FlushHW()
{
    SShader *ef = m_RP.m_pShader;
    if (!ef) return;

    SShaderTechnique *hs = m_RP.m_pCurTechnique;
    if (!hs) return;

    if (m_RP.m_pRE)
    {
        m_RP.m_pRE->mfCheckUpdate(ef->m_VertexFormatId, hs->m_Flags);
    }

    if (hs->m_Passes.Num())
    {
        for (int i=0; i<hs->m_Passes.Num(); i++)
        {
            SShaderPassHW *slw = &hs->m_Passes[i];

            // TODO: Setup pipeline state

            if (m_RP.m_pRE)
                m_RP.m_pRE->mfDraw(ef, slw);
            else
                EF_DrawIndexedMesh(R_PRIMV_TRIANGLES);
        }
    }
}

void CVulkanRenderer::EF_DrawIndexedMesh(int nPrimType)
{
    if (m_CurrentFrame >= MAX_FRAMES_IN_FLIGHT) return;
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    // 1. Setup Pipeline State
    VulkanPipelineState state;
    state.renderPass = m_RenderPass;
    // Fill state from m_RP (Render Pipeline state)
    // state.vertexShader = ...
    // state.fragmentShader = ...
    // state.renderState = ... (Cull mode, Depth test, etc.)
    state.vertexFormat = m_RP.m_CurVFormat;
    // state.topology = ... map nPrimType to VkPrimitiveTopology

    switch (nPrimType) {
        case R_PRIMV_TRIANGLES: state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
        case R_PRIMV_TRIANGLE_STRIP: state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
        default: state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
    }

    // 2. Get/Create Pipeline
    CVulkanPipeline* pPipeline = GetPipeline(state);
    if (pPipeline)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pPipeline->GetPipeline());
    }

    // 3. Draw
    // Buffers should be bound by EF_PreDraw
    if (m_RP.m_RendNumIndices > 0)
    {
        vkCmdDrawIndexed(cmd, m_RP.m_RendNumIndices, 1, m_RP.m_FirstIndex, m_RP.m_BaseVertex, 0);
    }
    else
    {
        vkCmdDraw(cmd, m_RP.m_RendNumVerts, 1, m_RP.m_FirstVertex, 0);
    }
}

void CVulkanRenderer::EF_DrawDebugTools()
{
    // TODO: Implement EF_DrawDebugTools
}

bool CVulkanRenderer::EF_PreDraw(SShaderPass *sl)
{
    if (m_RP.m_pRE)
    {
        return m_RP.m_pRE->mfPreDraw(sl);
    }
    else
    {
        // Dynamic/Immediate mode drawing (no RE)
        // TODO: Implement dynamic buffer handling
        return false;
    }
}

bool CVulkanRenderer::EF_ObjectChange(SShader *Shader, SRenderShaderResources *pRes, int nObject, CRendElement *pRE) { return true; }
int CVulkanRenderer::EF_Preprocess(SRendItemPre *ri, int nums, int nume) { return 0; }
void CVulkanRenderer::EF_DrawREPreprocess(SRendItemPreprocess *ris, int Nums) {}

bool CVulkanRenderer::FontUploadTexture(class CFBitmap*, ETEX_Format eTF) { return true; }
int CVulkanRenderer::FontCreateTexture(int Width, int Height, byte *pData, ETEX_Format eTF) { return 0; }
bool CVulkanRenderer::FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte *pData) { return true; }
void CVulkanRenderer::FontReleaseTexture(class CFBitmap *pBmp) {}
void CVulkanRenderer::FontSetTexture(class CFBitmap*, int nFilterMode) {}
void CVulkanRenderer::FontSetTexture(int nTexId, int nFilterMode) {}
void CVulkanRenderer::FontSetRenderingState(unsigned long nVirtualScreenWidth, unsigned long nVirtualScreenHeight) {}
void CVulkanRenderer::FontSetBlending(int src, int dst) {}
void CVulkanRenderer::FontRestoreRenderingState() {}
WIN_HWND CVulkanRenderer::GetHWND() { return m_hWnd; }

// Texture Helpers
void CVulkanRenderer::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo = {};
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

    if (vkCreateImage(m_Device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(m_Device, image, imageMemory, 0);
}

VkCommandBuffer CVulkanRenderer::BeginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void CVulkanRenderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_Queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Queue);

    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
}

void CVulkanRenderer::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier = {};
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
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
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

    EndSingleTimeCommands(commandBuffer);
}

void CVulkanRenderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(commandBuffer);
}

VkImageView CVulkanRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo = {};
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
    if (vkCreateImageView(m_Device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

CVulkanPipeline* CVulkanRenderer::GetPipeline(const VulkanPipelineState& state)
{
    auto it = m_Pipelines.find(state);
    if (it != m_Pipelines.end())
        return it->second;

    CVulkanPipeline* pPipeline = new CVulkanPipeline(m_Device, m_PipelineCache, state);
    if (pPipeline->Init())
    {
        m_Pipelines[state] = pPipeline;
        return pPipeline;
    }

    delete pPipeline;
    return NULL;
}

#include "Vulkan_Textures.cpp" // Include implementations of texture manager
#include "VulkanPipeline.cpp"
#include "VulkanRenderRE.cpp"

extern "C" DLL_EXPORT IRenderer * PackageRenderConstructor(int argc, char * argv[], SCryRenderInterface * sp)
{
    CVulkanRenderer *pRenderer = new CVulkanRenderer();
    if (sp)
    {
        pRenderer->m_pLog = sp->ipLog;
        pRenderer->m_pSystem = sp->ipSystem;
    }
    return pRenderer;
}
