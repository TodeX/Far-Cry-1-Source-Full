#include "../RenderPCH.h"
#include <windows.h> // Ensure windows.h is included for types
#include <stdexcept>
#include <cstring>
#include "VulkanRenderer.h"
#include <IConsole.h>
#include <ILog.h>
#include <set>
#include <algorithm>

CVulkanRenderer *gcpVulkan = NULL;

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
    , m_CurrentFrame(0)
    , m_ImageIndex(0)
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
        CreateCommandPool();
        CreateCommandBuffers();
        CreateSyncObjects();
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

    if (m_CommandPool) {
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;
    }

    if (m_PipelineCache) {
        vkDestroyPipelineCache(m_Device, m_PipelineCache, nullptr);
        m_PipelineCache = VK_NULL_HANDLE;
    }

    for (auto const& [key, val] : m_Pipelines) {
        delete val;
    }
    m_Pipelines.clear();

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
}

void CVulkanRenderer::Update()
{
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
    // TODO: Implement dynamic vertex buffer pointer retrieval
    // Should return a pointer to the mapped memory of the dynamic vertex buffer
    // and update the offset.
    return NULL;
}
void CVulkanRenderer::DrawDynVB(int nOffs, int Pool, int nVerts) {
    // TODO: Implement drawing from dynamic vertex buffer
    // Should bind the dynamic vertex buffer and issue a draw call
    // using the provided offset and vertex count.
}
void CVulkanRenderer::DrawDynVB(struct_VERTEX_FORMAT_P3F_COL4UB_TEX2F *pBuf, ushort *pInds, int nVerts, int nInds, int nPrimType) {
    // TODO: Implement drawing from dynamic vertex buffer with provided data and indices
    // Should probably copy data to a dynamic buffer and draw.
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
    // TODO: Implement drawing
    // Need pipeline, render pass, shaders bound.
    //
    // vkCmdBindVertexBuffers(m_CommandBuffers[m_CurrentFrame], 0, 1, &vertexBuffers, &offsets);
    // vkCmdBindIndexBuffer(m_CommandBuffers[m_CurrentFrame], indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    // vkCmdDrawIndexed(m_CommandBuffers[m_CurrentFrame], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
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

void CVulkanRenderer::Draw3dBBox(const Vec3 &mins, const Vec3 &maxs, int nPrimType) {}
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

void CVulkanRenderer::DrawTriStrip(CVertexBuffer *src, int vert_num) {
    // TODO: Implement triangle strip drawing
    // Should bind the vertex buffer from src and issue a draw call
    // with VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP.
}

// Matrix operations
void CVulkanRenderer::PushMatrix() {}
void CVulkanRenderer::RotateMatrix(float a,float x,float y,float z) {}
void CVulkanRenderer::RotateMatrix(const Vec3 & angels) {}
void CVulkanRenderer::TranslateMatrix(float x,float y,float z) {}
void CVulkanRenderer::ScaleMatrix(float x,float y,float z) {}
void CVulkanRenderer::TranslateMatrix(const Vec3 &pos) {}
void CVulkanRenderer::MultMatrix(float * mat) {}
void CVulkanRenderer::LoadMatrix(const Matrix44 *src) {}
void CVulkanRenderer::PopMatrix() {}

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

void CVulkanRenderer::Draw2dImage(float xpos,float ypos,float w,float h,int texture_id,float s0,float t0,float s1,float t1,float angle,float r,float g,float b,float a,float z) {}

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
void CVulkanRenderer::EF_EndEf3D (int nFlags) {}
void CVulkanRenderer::EF_EndEf2D(bool bSort) {}
int CVulkanRenderer::EF_RegisterFogVolume(float fMaxFogDist, float fFogLayerZ, CFColor color, int nIndex, bool bCaustics) { return 0; }

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
