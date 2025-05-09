//
// square.c
//
//  Copyright © 2025 Robert Guequierre
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include <vulkan/vulkan.h>
#include <tiffio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utilities.h"

//====----------------------------------------------------------------------====
//
// * Shader data
//
//====----------------------------------------------------------------------====

const uint8_t alignas(uint32_t) vertexShaderData[] = {
    #embed "vertex.spv"
};

const uint8_t alignas(uint32_t) fragmentShaderData[] = {
    #embed "fragment.spv"
};

//====----------------------------------------------------------------------====
//
// * ImageContext
//
//====----------------------------------------------------------------------====

typedef struct ImageContext
{
    uint32_t        width;
    uint32_t        height;
    VkDeviceSize    bytesPerRow;
    VkFormat        colorPixelFormat;
    uint8_t*        data;
}
ImageContext;

// * disposeImageContext
//
void disposeImageContext(ImageContext* ctx)
{
    free(ctx->data);

    memset( ctx, 0, sizeof(*ctx) );
}

//====----------------------------------------------------------------------====
// renderImage
//====----------------------------------------------------------------------====

ImageContext renderImage(uint32_t width, uint32_t height)
{
    ImageContext imageContext = {};

    //====------------------------------------------------------------------====
    // * Common layer names
    //
    const char* layerNames[] = { "VK_LAYER_KHRONOS_validation" };

    //====------------------------------------------------------------------====
    // * Instance

    //  - application info
    const VkApplicationInfo applicationInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext              = nullptr,
        .pApplicationName   = "base",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "no engine",
        .engineVersion      = VK_MAKE_VERSION(0, 0, 0),
        .apiVersion         = VK_API_VERSION_1_4
    };

    //  - instance
    const VkInstanceCreateInfo instanceInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .pApplicationInfo        = &applicationInfo,
        .enabledLayerCount       = ARRAY_LENGTH(layerNames),
        .ppEnabledLayerNames     = layerNames,
        .enabledExtensionCount   = 0,
        .ppEnabledExtensionNames = nullptr
    };

    VkInstance instance = nullptr;

    auto result = vkCreateInstance(&instanceInfo, nullptr, &instance);

    if (VK_SUCCESS != result) {
        goto post_cleanup_instance;
    }

    //====------------------------------------------------------------------====
    // * Physical device
    //
    VkPhysicalDevice physicalDevice = nullptr;

    result = findFirstGPU(instance, &physicalDevice);

    if (VK_SUCCESS != result) {
        goto post_cleanup_physical_device;
    }

    //  - memory properties
    VkPhysicalDeviceMemoryProperties memoryProperties = {};

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    //====------------------------------------------------------------------====
    // * Queue family
    //
    uint32_t queueFamilyIndex = 0;

    result = findGraphicsAndComputeQueueFamily(physicalDevice, &queueFamilyIndex);

    if (VK_SUCCESS != result) {
        goto post_cleanup_queue_family_index;
    }

    //====------------------------------------------------------------------====
    // * Logical device

    //  - device queue
    auto const queuePriority = 1.0f;

    const VkDeviceQueueCreateInfo deviceQueueInfo = {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount       = 1,
        .pQueuePriorities = &queuePriority
    };

    //  - physical device features
    const VkPhysicalDeviceFeatures physicalDeviceFeatures = {};

    //  - device
    const VkDeviceCreateInfo deviceInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &deviceQueueInfo,
        .enabledLayerCount       = ARRAY_LENGTH(layerNames),
        .ppEnabledLayerNames     = layerNames,
        .enabledExtensionCount   = 0,
        .ppEnabledExtensionNames = nullptr,
        .pEnabledFeatures        = &physicalDeviceFeatures
    };

    VkDevice device = nullptr;

    result = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);

    if (VK_SUCCESS != result) {
        goto post_cleanup_device;
    }

    //  - queue
    //
    VkQueue queue = nullptr;

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    //====------------------------------------------------------------------====
    // * Command pool
    //
    const VkCommandPoolCreateInfo commandPoolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };

    VkCommandPool commandPool = nullptr;

    result = vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool);

    if (VK_SUCCESS != result) {
        goto post_cleanup_command_pool;
    }

    //====------------------------------------------------------------------====
    // * Pipeline cache
    //
    const VkPipelineCacheCreateInfo pipelineCacheInfo = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .initialDataSize = 0,
        .pInitialData    = nullptr
    };

    VkPipelineCache pipelineCache = nullptr;

    result = vkCreatePipelineCache( device, &pipelineCacheInfo, nullptr,
                                    &pipelineCache );
    if (VK_SUCCESS != result) {
        goto post_cleanup_pipeline_cache;
    }

    //====------------------------------------------------------------------====
    // * Shaders
    
    //  - vertex
    const VkShaderModuleCreateInfo vertexShaderInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .codeSize = sizeof(vertexShaderData),
        .pCode    = (const uint32_t*)vertexShaderData
    };

    VkShaderModule vertexShader = nullptr;

    result = vkCreateShaderModule( device, &vertexShaderInfo, nullptr,
                                   &vertexShader );

    if (VK_SUCCESS != result) {
        goto post_cleanup_vertex_shader;
    }

    //  - fragment
    const VkShaderModuleCreateInfo fragmentShaderInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext    = nullptr,
        .flags    = 0,
        .codeSize = sizeof(fragmentShaderData),
        .pCode    = (const uint32_t*)fragmentShaderData
    };

    VkShaderModule fragmentShader = nullptr;

    result = vkCreateShaderModule( device, &fragmentShaderInfo, nullptr,
                                   &fragmentShader );

    if (VK_SUCCESS != result) {
        goto post_cleanup_fragment_shader;
    }

    //  - stages
    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_VERTEX_BIT,
            .module              = vertexShader,
            .pName               = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module              = fragmentShader,
            .pName               = "main",
            .pSpecializationInfo = nullptr
        }
    };

    //====------------------------------------------------------------------====
    // * Fixed function settings

    //  - vertex input
    const VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = nullptr
    };

    //  - input assembly
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitiveRestartEnable = false
    };

    //  - viewport
    const VkViewport viewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float)width,
        .height   = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    const VkRect2D scissor = {
        .offset = { 0, 0 }, 
        .extent = { width, height } 
    };

    const VkPipelineViewportStateCreateInfo viewportInfo = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor
    };

    //  - rasterization
    const VkPipelineRasterizationStateCreateInfo rasterizationInfo = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .depthClampEnable        = false,
        .rasterizerDiscardEnable = false,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE, 
        .depthBiasEnable         = false,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp          = 0.0f,
        .depthBiasSlopeFactor    = 0.0f,
        .lineWidth               = 1.0f
    };

    //  - multisampling
    const VkPipelineMultisampleStateCreateInfo multisamplingInfo = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable   = false,
        .minSampleShading      = 1.0f,
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = false,
        .alphaToOneEnable      = false
    };

    //  - blend mode
    const VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable         = false,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT
                             | VK_COLOR_COMPONENT_G_BIT
                             | VK_COLOR_COMPONENT_B_BIT
                             | VK_COLOR_COMPONENT_A_BIT
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendInfo = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .logicOpEnable   = false,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment,
        .blendConstants  = { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    //  - dynamic states
    const VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext             = nullptr,
        .flags             = 0,
        .dynamicStateCount = 0,
        .pDynamicStates    = nullptr
    };

    //====------------------------------------------------------------------====
    // * Pipeline layout
    //
    const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .setLayoutCount         = 0,
        .pSetLayouts            = nullptr,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = nullptr
    };

    VkPipelineLayout pipelineLayout = nullptr;

    result = vkCreatePipelineLayout( device, &pipelineLayoutInfo, nullptr,
                                     &pipelineLayout );
    if (VK_SUCCESS != result) {
        goto post_cleanup_pipeline_layout;
    }

    //====------------------------------------------------------------------====
    // * Render pass

    //  - color attachment
    const VkAttachmentDescription colorAttachment = {
        .flags          = 0,
        .format         = VK_FORMAT_R8G8B8A8_UNORM,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    };

    //  - subpass
    const VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkSubpassDescription subpass = {
        .flags                   = 0,
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount    = 0,
        .pInputAttachments       = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colorAttachmentRef,
        .pResolveAttachments     = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments    = nullptr
    };

    //  - subpass dependency : post-image render only. A second, preceding
    //                         dependency would be added for copying, for
    //                         example, vertex buffer data to the device
    const VkSubpassDependency subpassDependency = {
        .srcSubpass      = 0,
        .dstSubpass      = VK_SUBPASS_EXTERNAL,
        .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                         | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    };

    //  - render pass
    const VkRenderPassCreateInfo renderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .attachmentCount = 1,
        .pAttachments    = &colorAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &subpassDependency
    };

    VkRenderPass renderPass = nullptr;

    result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);

    if (VK_SUCCESS != result) {
        goto post_cleanup_render_pass;
    }

    //====--------------------------------------------------------------====
    // * Pipeline
    //
    const VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = nullptr,
        .flags               = 0,
        .stageCount          = ARRAY_LENGTH(shaderStages),
        .pStages             = shaderStages,
        .pVertexInputState   = &vertexInputInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewportInfo,
        .pRasterizationState = &rasterizationInfo,
        .pMultisampleState   = &multisamplingInfo,
        .pDepthStencilState  = nullptr,
        .pColorBlendState    = &colorBlendInfo,
        .pDynamicState       = &dynamicStateInfo,
        .layout              = pipelineLayout,
        .renderPass          = renderPass,
        .subpass             = 0,
        .basePipelineHandle  = nullptr,
        .basePipelineIndex   = -1
    };

    VkPipeline graphicsPipeline = nullptr;

    result = vkCreateGraphicsPipelines( device, pipelineCache, 1, &pipelineInfo, 
                                        nullptr, &graphicsPipeline );
    if (VK_SUCCESS != result) {
        goto post_cleanup_graphics_pipeline;
    }

    //  - shader modules no longer in use
    vkDestroyShaderModule(device, fragmentShader, nullptr);
    fragmentShader = nullptr;

    vkDestroyShaderModule(device, vertexShader, nullptr);
    vertexShader = nullptr;

    //====--------------------------------------------------------------====
    // * Image

    //  - image
    const VkImageCreateInfo imageInfo = {
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = colorAttachment.format,
        .extent                = { width, height, 1 },
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_OPTIMAL,
        .usage                 = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                               | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage        image       = nullptr;
    VkDeviceMemory imageMemory = nullptr;

    result = createImageAndMemory( device, &imageInfo, &memoryProperties,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   &image, &imageMemory );
    if (VK_SUCCESS != result) {
        goto post_cleanup_image;
    }

    //  - image view
    const VkImageViewCreateInfo imageViewInfo = {
        .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext      = nullptr,
        .flags      = 0,
        .image      = image,
        .viewType   = VK_IMAGE_VIEW_TYPE_2D,
        .format     = imageInfo.format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    VkImageView imageView = nullptr;

    result = vkCreateImageView(device, &imageViewInfo, nullptr, &imageView);

    if (VK_SUCCESS != result) {
        goto post_cleanup_image_view;
    }

    //  - framebuffer
    const VkFramebufferCreateInfo framebufferInfo = {
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .renderPass      = renderPass,
        .attachmentCount = 1,
        .pAttachments    = &imageView,
        .width           = width,
        .height          = height,
        .layers          = 1
    };

    VkFramebuffer framebuffer = nullptr;

    result = vkCreateFramebuffer( device, &framebufferInfo, nullptr,
                                  &framebuffer );
    if (VK_SUCCESS != result) {
        goto post_cleanup_framebuffer;
    }

    //====-----------------------------------------------------------------====
    // * Command buffer

    //  - allocate
    const VkCommandBufferAllocateInfo renderCommandBufferInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer renderCommandBuffer = nullptr;

    result = vkAllocateCommandBuffers( device, &renderCommandBufferInfo,
                                       &renderCommandBuffer );
    if (VK_SUCCESS != result) {
        goto post_cleanup_render_command_buffer;
    }

    //  - begin
    const VkCommandBufferBeginInfo renderCommandBufferBeginInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .pInheritanceInfo = nullptr
    };

    result = vkBeginCommandBuffer(renderCommandBuffer, &renderCommandBufferBeginInfo);

    if (VK_SUCCESS != result) {
        goto post_render_command;
    }

    //  - render pass
    const VkClearValue clearValues[] = {
        { .color = { .float32 = { 0.1f, 0.0f, 0.1f, 1.0f } } }
    };

    const VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext       = nullptr,
        .renderPass  = renderPass,
        .framebuffer = framebuffer,
        .renderArea  = { 
            .offset = { 0, 0 }, 
            .extent = { width, height } 
        },
        .clearValueCount = ARRAY_LENGTH(clearValues),
        .pClearValues    = clearValues
    };

    vkCmdBeginRenderPass( renderCommandBuffer, &renderPassBeginInfo,
                          VK_SUBPASS_CONTENTS_INLINE );

    //  - pipeline
    vkCmdBindPipeline( renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                       graphicsPipeline );

    //  - draw
    vkCmdDraw(renderCommandBuffer, 4, 1, 0, 0);

    //  - end
    vkCmdEndRenderPass(renderCommandBuffer);

    result = vkEndCommandBuffer(renderCommandBuffer);

    if (VK_SUCCESS != result) {
        goto post_render_command;
    }

    //  - submit command buffer
    result = submitCommandBuffer(device, queue, renderCommandBuffer);

    if (VK_SUCCESS != result) {
        goto post_render_command;
    }

    //  - wait for events to complete
    vkQueueWaitIdle(queue);

    //  - command buffer no longer in use
    vkFreeCommandBuffers(device, commandPool, 1, &renderCommandBuffer);
    renderCommandBuffer = nullptr;

    //====------------------------------------------------------------------====
    // * Destination image

    //  - image
    const VkImageCreateInfo destImageInfo = {
        .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .imageType             = VK_IMAGE_TYPE_2D,
        .format                = VK_FORMAT_R8G8B8A8_UNORM,
        .extent                = { width, height, 1 },
        .mipLevels             = 1,
        .arrayLayers           = 1,
        .samples               = VK_SAMPLE_COUNT_1_BIT,
        .tiling                = VK_IMAGE_TILING_LINEAR,
        .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices   = nullptr,
        .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage        destImage       = nullptr;
    VkDeviceMemory destImageMemory = nullptr;

    result = createImageAndMemory( device, &destImageInfo, &memoryProperties,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   &destImage,
                                   &destImageMemory );
    if (VK_SUCCESS != result) {
        goto post_cleanup_dest_image;
    }

    //====------------------------------------------------------------------====
    // * Copy command

    //  - command buffer
    const VkCommandBufferAllocateInfo copyCommandBufferInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer copyCommandBuffer = nullptr;

    result = vkAllocateCommandBuffers( device, &copyCommandBufferInfo,
                                       &copyCommandBuffer );
    if (VK_SUCCESS != result) {
        goto post_cleanup_copy_command_buffer;
    }

    //  - begin
    const VkCommandBufferBeginInfo copyCommandBufferBeginInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .pInheritanceInfo = nullptr
    };

    result = vkBeginCommandBuffer(copyCommandBuffer, &copyCommandBufferBeginInfo);

    if (VK_SUCCESS != result) {
        goto post_copy_command;
    }

    //  - transition destination image to transfer destination layout
    const VkImageMemoryBarrier destLayoutBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = 0,
        .dstQueueFamilyIndex = 0,
        .image               = destImage,
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    vkCmdPipelineBarrier( copyCommandBuffer,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          0, 0, nullptr, 0, nullptr,
                          1, &destLayoutBarrier );

    //  - copy image
    const VkImageCopy imageCopy = {
        .srcSubresource = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1
        },
        .srcOffset      = { .x = 0, .y = 0, .z = 0 },
        .dstSubresource = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel       = 0,
            .baseArrayLayer = 0,
            .layerCount     = 1
        },
        .dstOffset = { .x = 0, .y = 0, .z = 0 },
        .extent    = { width, height, 1 }
    };

    vkCmdCopyImage( copyCommandBuffer,
                    image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &imageCopy );

    //  - transition destination image to general layout
    const VkImageMemoryBarrier generalLayoutBarrier = {
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = 0,
        .dstQueueFamilyIndex = 0,
        .image               = destImage,
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    vkCmdPipelineBarrier( copyCommandBuffer,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          0, 0, nullptr, 0, nullptr,
                          1, &generalLayoutBarrier );

    //  - end
    result = vkEndCommandBuffer(copyCommandBuffer);

    if (VK_SUCCESS != result) {
        goto post_copy_command;
    }

    //  - submit copy command buffer
    result = submitCommandBuffer(device, queue, copyCommandBuffer);

    if (VK_SUCCESS != result) {
        goto post_copy_command;
    }

    //  - wait for events to complete
    vkQueueWaitIdle(queue);

    //  - command buffer no longer in use
    vkFreeCommandBuffers(device, commandPool, 1, &copyCommandBuffer);
    copyCommandBuffer = nullptr;

    //====--------------------------------------------------------------====
    // * Copy destination image to host allocated buffer

    //  - image memory layout
    const VkImageSubresource destImageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel   = 0,
        .arrayLayer = 0
    };

    VkSubresourceLayout destImageSubresourceLayout = {};

    vkGetImageSubresourceLayout( device, destImage,
                                 &destImageSubresource,
                                 &destImageSubresourceLayout );
    //  - map memory
    uint8_t* pDestImageData = nullptr;

    result = vkMapMemory( device, destImageMemory, 0, VK_WHOLE_SIZE, 0,
                          (void**)&pDestImageData );

    if (VK_SUCCESS != result) {
        goto post_copy_command;
    }

    pDestImageData += destImageSubresourceLayout.offset;

    //  - copy to local buffer
    size_t destImageDataSize = (size_t)height * destImageSubresourceLayout.rowPitch;

    imageContext.data = malloc(destImageDataSize);

    if (nullptr != imageContext.data)
    {
        memcpy(imageContext.data, pDestImageData, destImageDataSize);

        imageContext.width            = width;
        imageContext.height           = height;
        imageContext.colorPixelFormat = destImageInfo.format;
        imageContext.bytesPerRow      = destImageSubresourceLayout.rowPitch;
    }

    vkUnmapMemory(device, destImageMemory);
    pDestImageData = nullptr;

    //====------------------------------------------------------------------====
    // * Cleanup
    //

post_copy_command:

    if (nullptr != copyCommandBuffer) {
        vkFreeCommandBuffers(device, commandPool, 1, &copyCommandBuffer);
        copyCommandBuffer = nullptr;
    }

post_cleanup_copy_command_buffer:

    vkDestroyImage(device, destImage, nullptr);
    destImage = nullptr;

    vkFreeMemory(device, destImageMemory, nullptr);
    destImageMemory = nullptr;

post_cleanup_dest_image:
post_render_command:

    if (nullptr != renderCommandBuffer) {
        vkFreeCommandBuffers(device, commandPool, 1, &renderCommandBuffer);
        renderCommandBuffer = nullptr;
    }

post_cleanup_render_command_buffer:

    vkDestroyFramebuffer(device, framebuffer, nullptr);
    framebuffer = nullptr;

post_cleanup_framebuffer:

    vkDestroyImageView(device, imageView, nullptr);
    imageView = nullptr;

post_cleanup_image_view:

    vkDestroyImage(device, image, nullptr);
    image = nullptr;

    vkFreeMemory(device, imageMemory, nullptr);
    imageMemory = nullptr;

post_cleanup_image:

    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    graphicsPipeline = nullptr;

post_cleanup_graphics_pipeline:

    vkDestroyRenderPass(device, renderPass, nullptr);
    renderPass = nullptr;

post_cleanup_render_pass:

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    pipelineLayout = nullptr;

post_cleanup_pipeline_layout:

    if (nullptr != fragmentShader) {
        vkDestroyShaderModule(device, fragmentShader, nullptr);
        fragmentShader = nullptr;
    }

post_cleanup_fragment_shader:

    if (nullptr != vertexShader) {
        vkDestroyShaderModule(device, vertexShader, nullptr);
        vertexShader = nullptr;
    }

post_cleanup_vertex_shader:

    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    pipelineCache = nullptr;

post_cleanup_pipeline_cache:

    vkDestroyCommandPool(device, commandPool, nullptr);
    commandPool = nullptr;

post_cleanup_command_pool:

    vkDestroyDevice(device, nullptr);
    device = nullptr;
    queue  = nullptr;

post_cleanup_device:

    queueFamilyIndex = 0;

post_cleanup_queue_family_index:

    physicalDevice = nullptr;

post_cleanup_physical_device:

    vkDestroyInstance(instance, nullptr);
    instance = nullptr;

post_cleanup_instance:

    return imageContext;
}

//====----------------------------------------------------------------------====
// * saveRGBATIFFFile
//====----------------------------------------------------------------------====

bool saveRGBATIFFFile( const char*    filename,
                       const uint8_t* imageData,
                       uint32_t       width,
                       uint32_t       height,
                       size_t         bytesPerRow )
{
    auto file = TIFFOpen(filename, "w");

    if (nullptr == file) {
        return false;
    }

    //  - image properties
    TIFFSetField(file, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(file, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(file, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(file, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    const uint16_t extraSample = EXTRASAMPLE_ASSOCALPHA;
    TIFFSetField(file, TIFFTAG_EXTRASAMPLES, 1, &extraSample);

    TIFFSetField( file, TIFFTAG_ROWSPERSTRIP,
                  TIFFDefaultStripSize(file, 4*width) );

    //  - scan line buffer
    auto const preferredScanlineSize = (size_t)TIFFScanlineSize(file);
    
    auto const scanlineSize = (0 < preferredScanlineSize)
                               ? preferredScanlineSize
                               : bytesPerRow;

    auto scanlineBuffer = (uint8_t*)_TIFFmalloc(scanlineSize);
    auto result         = false;

    if (nullptr != scanlineBuffer)
    {
        //  - write each scanline
        auto const rowCopySize = (scanlineSize < bytesPerRow)
                                  ? scanlineSize
                                  : bytesPerRow;

        const uint8_t* row         = imageData;
        int            writeResult = -1;

        for (uint32_t yy = 0; yy < height; ++yy)
        {
            memcpy(scanlineBuffer, row, rowCopySize);            

            writeResult = TIFFWriteScanline(file, scanlineBuffer, yy, 0);

            if (writeResult < 0) {
                break;
            }

            row += bytesPerRow;
        }

        //  - cleanup buffer
        _TIFFfree(scanlineBuffer);
        scanlineBuffer = nullptr;

        //  - result
        result = (0 <= writeResult);
    }

    //  - cleanup file
    TIFFClose(file);
    file = nullptr;

    return result;
}

//====----------------------------------------------------------------------====
// * main
//====----------------------------------------------------------------------====

int main( [[maybe_unused]] const int         argc, 
          [[maybe_unused]] const char* const argv[] )
{
    // * Render image
    //
    auto imageContext = renderImage(1080, 1080);

    if (nullptr == imageContext.data)
    {
        puts("Failed to render image");
        return EXIT_FAILURE;
    }

    // * Save image file
    //
    auto didSave = saveRGBATIFFFile( "output.tiff",
                                     imageContext.data,
                                     imageContext.width,
                                     imageContext.height,
                                     imageContext.bytesPerRow );
    // * Cleanup
    //
    disposeImageContext(&imageContext);

    // * Error reporting
    //
    if (!didSave) 
    {
        puts("Failed to save image file");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

