#include "../RenderPCH.h"
#include "VulkanRenderer.h"
#include "VulkanPipeline.h"

// --------------------------------------------------------------------------------
// CVulkanPipeline Implementation
// --------------------------------------------------------------------------------

CVulkanPipeline::CVulkanPipeline(VkDevice device, VkPipelineCache cache, const VulkanPipelineState& state)
    : m_Device(device)
    , m_Cache(cache)
    , m_State(state)
    , m_Pipeline(VK_NULL_HANDLE)
    , m_PipelineLayout(VK_NULL_HANDLE)
{
}

CVulkanPipeline::~CVulkanPipeline()
{
    if (m_Pipeline)
    {
        vkDestroyPipeline(m_Device, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout)
    {
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
}

bool CVulkanPipeline::Init()
{
    CreateLayout();
    CreatePipeline();
    return (m_Pipeline != VK_NULL_HANDLE);
}

void CVulkanPipeline::CreateLayout()
{
    // Enable Push Constants for MVP matrix (and other data)
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 128; // Standard minimum guarantee

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        // gRenDev->CheckError("Failed to create pipeline layout!");
    }
}

void CVulkanPipeline::CreatePipeline()
{
    // --------------------------------------------------------------------------------
    // Shader Stages
    // --------------------------------------------------------------------------------
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    // Vertex Shader
    if (m_State.vertexShader != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = m_State.vertexShader;
        vertShaderStageInfo.pName = "main";
        shaderStages.push_back(vertShaderStageInfo);
    }

    // Fragment Shader
    if (m_State.fragmentShader != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = m_State.fragmentShader;
        fragShaderStageInfo.pName = "main";
        shaderStages.push_back(fragShaderStageInfo);
    }

    // --------------------------------------------------------------------------------
    // Vertex Input State
    // --------------------------------------------------------------------------------
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // Map m_State.vertexFormat to Vulkan vertex attributes and bindings.
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

    if (m_State.vertexFormat > 0 && m_State.vertexFormat < VERTEX_FORMAT_NUMS)
    {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = m_VertexSize[m_State.vertexFormat];
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions.push_back(bindingDescription);

        // Position (Location 0)
        VkVertexInputAttributeDescription posAttr = {};
        posAttr.binding = 0;
        posAttr.location = 0;
        // Check for TRP3F (Transformed Position, usually x,y,z,rhw)
        if (m_State.vertexFormat == VERTEX_FORMAT_TRP3F_COL4UB_TEX2F)
            posAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        else
            posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
        posAttr.offset = 0;
        attributeDescriptions.push_back(posAttr);

        SBufInfoTable& info = gBufInfoTable[m_State.vertexFormat];

        // Color (Location 1)
        if (info.OffsColor) {
            VkVertexInputAttributeDescription colAttr = {};
            colAttr.binding = 0;
            colAttr.location = 1;
            colAttr.format = VK_FORMAT_B8G8R8A8_UNORM;
            colAttr.offset = info.OffsColor;
            attributeDescriptions.push_back(colAttr);
        }

        // TexCoord (Location 2)
        if (info.OffsTC) {
            VkVertexInputAttributeDescription tcAttr = {};
            tcAttr.binding = 0;
            tcAttr.location = 2;
            tcAttr.format = VK_FORMAT_R32G32_SFLOAT;
            tcAttr.offset = info.OffsTC;
            attributeDescriptions.push_back(tcAttr);
        }

        // Normal (Location 3)
        if (info.OffsNormal) {
            VkVertexInputAttributeDescription normAttr = {};
            normAttr.binding = 0;
            normAttr.location = 3;
            normAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
            normAttr.offset = info.OffsNormal;
            attributeDescriptions.push_back(normAttr);
        }

        // Secondary Color (Location 4)
        if (info.OffsSecColor) {
            VkVertexInputAttributeDescription secColAttr = {};
            secColAttr.binding = 0;
            secColAttr.location = 4;
            secColAttr.format = VK_FORMAT_B8G8R8A8_UNORM;
            secColAttr.offset = info.OffsSecColor;
            attributeDescriptions.push_back(secColAttr);
        }
    }

    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // --------------------------------------------------------------------------------
    // Input Assembly State
    // --------------------------------------------------------------------------------
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = m_State.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE; // Should be true for Strips with restart index

    // --------------------------------------------------------------------------------
    // Viewport State (Dynamic)
    // --------------------------------------------------------------------------------
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr; // Ignored if dynamic
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr; // Ignored if dynamic

    // --------------------------------------------------------------------------------
    // Rasterization State
    // --------------------------------------------------------------------------------
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Default, change if wireframe requested in m_State.renderState
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;   // Default, change based on m_State.renderState
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; // Check Engine convention (usually CCW in OGL, but check projection)
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    // Map Render State flags to Rasterizer State
    // if (m_State.renderState & GS_WIREFRAME) rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    // if (m_State.renderState & GS_NOCULL) rasterizer.cullMode = VK_CULL_MODE_NONE;

    // --------------------------------------------------------------------------------
    // Multisample State
    // --------------------------------------------------------------------------------
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // --------------------------------------------------------------------------------
    // Depth Stencil State
    // --------------------------------------------------------------------------------
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;  // Default
    depthStencil.depthWriteEnable = VK_TRUE; // Default
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Map Render State flags to Depth Stencil State
    // if (m_State.renderState & GS_NODEPTHTEST) depthStencil.depthTestEnable = VK_FALSE;
    // if (m_State.renderState & GS_NODEPTHWRITE) depthStencil.depthWriteEnable = VK_FALSE;

    // Stencil state mapping
    if (m_State.stencilState != 0) // Simplified check
    {
        depthStencil.stencilTestEnable = VK_TRUE;
        // Map m_State.stencilState bits to front/back stencil ops
    }

    // --------------------------------------------------------------------------------
    // Color Blend State
    // --------------------------------------------------------------------------------
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    // Map Render State flags to Blend State
    // if (m_State.renderState & GS_BLEND) {
    //     colorBlendAttachment.blendEnable = VK_TRUE;
    //     // extract src/dst factors from m_State.renderState
    // }

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // --------------------------------------------------------------------------------
    // Dynamic State
    // --------------------------------------------------------------------------------
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // --------------------------------------------------------------------------------
    // Create Pipeline
    // --------------------------------------------------------------------------------
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = m_State.renderPass;
    pipelineInfo.subpass = m_State.subpass;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(m_Device, m_Cache, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS) {
        // gRenDev->CheckError("Failed to create graphics pipeline!");
    }
}
