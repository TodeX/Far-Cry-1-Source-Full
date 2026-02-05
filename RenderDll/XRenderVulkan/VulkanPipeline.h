#ifndef VULKAN_PIPELINE_H
#define VULKAN_PIPELINE_H

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vector>
#include <cstring>

// State description for pipeline creation
// This should match the information available in the engine's Render State
struct VulkanPipelineState {
    // Shader Modules (Placeholder types or VkShaderModule handles)
    VkShaderModule vertexShader;
    VkShaderModule fragmentShader;

    // Render State flags (GS_* flags)
    uint32_t renderState;
    uint32_t stencilState;
    uint32_t stencilReadMask;
    uint32_t stencilWriteMask;

    // Vertex Input state (Format ID)
    uint32_t vertexFormat;

    // Topology
    VkPrimitiveTopology topology;

    // Render Pass compatibility
    VkRenderPass renderPass;
    uint32_t subpass;

    // Default constructor
    VulkanPipelineState() :
        vertexShader(VK_NULL_HANDLE),
        fragmentShader(VK_NULL_HANDLE),
        renderState(0),
        stencilState(0),
        stencilReadMask(0xFF),
        stencilWriteMask(0xFF),
        vertexFormat(0),
        topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
        renderPass(VK_NULL_HANDLE),
        subpass(0)
    {}

    // Equality operator for hashing/map
    bool operator==(const VulkanPipelineState& other) const {
        return vertexShader == other.vertexShader &&
               fragmentShader == other.fragmentShader &&
               renderState == other.renderState &&
               stencilState == other.stencilState &&
               stencilReadMask == other.stencilReadMask &&
               stencilWriteMask == other.stencilWriteMask &&
               vertexFormat == other.vertexFormat &&
               topology == other.topology &&
               renderPass == other.renderPass &&
               subpass == other.subpass;
    }

    // Less-than operator for std::map
    bool operator<(const VulkanPipelineState& other) const {
        if (vertexShader != other.vertexShader) return vertexShader < other.vertexShader;
        if (fragmentShader != other.fragmentShader) return fragmentShader < other.fragmentShader;
        if (renderState != other.renderState) return renderState < other.renderState;
        if (stencilState != other.stencilState) return stencilState < other.stencilState;
        if (stencilReadMask != other.stencilReadMask) return stencilReadMask < other.stencilReadMask;
        if (stencilWriteMask != other.stencilWriteMask) return stencilWriteMask < other.stencilWriteMask;
        if (vertexFormat != other.vertexFormat) return vertexFormat < other.vertexFormat;
        if (topology != other.topology) return topology < other.topology;
        if (renderPass != other.renderPass) return renderPass < other.renderPass;
        return subpass < other.subpass;
    }
};

class CVulkanPipeline {
public:
    CVulkanPipeline(VkDevice device, VkPipelineCache cache, const VulkanPipelineState& state);
    ~CVulkanPipeline();

    VkPipeline GetPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetLayout() const { return m_PipelineLayout; }

    bool Init();

private:
    VkDevice m_Device;
    VkPipelineCache m_Cache;
    VulkanPipelineState m_State;
    VkPipeline m_Pipeline;
    VkPipelineLayout m_PipelineLayout;

    void CreateLayout();
    void CreatePipeline();
};

#endif // VULKAN_PIPELINE_H
