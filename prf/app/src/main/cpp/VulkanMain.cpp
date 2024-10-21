// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "VulkanMain.hpp"

#include "log.h"

// Vulkan相关
#include "vulkan/utils.h"
#include "vulkan/instance.h"
#include "vulkan/surface.h"
#include "vulkan/physical_device.h"
#include "vulkan/queue_family_index.h"
#include "vulkan/device.h"
#include "vulkan/queue.h"
#include "vulkan/swapchain.h"
#include "vulkan/render_pass.h"
#include "vulkan/frame_buffers.h"
#include "vulkan/command_pool.h"
#include "vulkan/command_buffers.h"

#include <vulkan_wrapper.h>

#include <cassert>
#include <cstring>
#include <vector>

VulkanDeviceInfo device;
VulkanSwapchainInfo swapchain;


// 顶点缓冲
struct VulkanBufferInfo {
    VkBuffer vertexBuf_;
};
VulkanBufferInfo buffers;

// 渲染管线信息
struct VulkanGfxPipelineInfo {
    VkPipelineLayout layout_;
    VkPipelineCache cache_; // 对于pipeline的缓存？
    VkPipeline pipeline_;
};
VulkanGfxPipelineInfo gfxPipeline;

VulkanRenderInfo render;

// Android Native App pointer...
android_app *androidAppCtx = nullptr;

/*
 * setImageLayout():
 *    Helper function to transition color buffer layout
 */
void setImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
                    VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                    VkPipelineStageFlags srcStages,
                    VkPipelineStageFlags destStages);

// A helper function
bool MapMemoryTypeToIndex(uint32_t typeBits, VkFlags requirements_mask,
                          uint32_t *typeIndex) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(device.gpuDevice_, &memoryProperties);
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memoryProperties.memoryTypes[i].propertyFlags & requirements_mask) ==
                requirements_mask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    return false;
}

// Create our vertex buffer
bool CreateBuffers(void) {
    // -----------------------------------------------
    // Create the triangle vertex buffer

    // Vertex positions
    const float vertexData[] = {
            -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    };

    // Create a vertex buffer
    VkBufferCreateInfo createBufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sizeof(vertexData),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &device.queueFamilyIndex_,
    };

    CALL_VK(vkCreateBuffer(device.device_, &createBufferInfo, nullptr,
                           &buffers.vertexBuf_));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device.device_, buffers.vertexBuf_, &memReq);

    VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReq.size,
            .memoryTypeIndex = 0,  // Memory type assigned in the next step
    };

    // Assign the proper memory type for that buffer
    MapMemoryTypeToIndex(memReq.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         &allocInfo.memoryTypeIndex);

    // Allocate memory for the buffer
    VkDeviceMemory deviceMemory;
    CALL_VK(vkAllocateMemory(device.device_, &allocInfo, nullptr, &deviceMemory));

    void *data;
    CALL_VK(vkMapMemory(device.device_, deviceMemory, 0, allocInfo.allocationSize,
                        0, &data));
    memcpy(data, vertexData, sizeof(vertexData));
    vkUnmapMemory(device.device_, deviceMemory);

    CALL_VK(
            vkBindBufferMemory(device.device_, buffers.vertexBuf_, deviceMemory, 0));
    return true;
}

void DeleteBuffers(void) {
    vkDestroyBuffer(device.device_, buffers.vertexBuf_, nullptr);
}

enum ShaderType {
    VERTEX_SHADER, FRAGMENT_SHADER
};

VkResult loadShaderFromFile(const char *filePath, VkShaderModule *shaderOut,
                            ShaderType type) {
    // Read the file
    assert(androidAppCtx);
    AAsset *file = AAssetManager_open(androidAppCtx->activity->assetManager,
                                      filePath, AASSET_MODE_BUFFER);
    size_t fileLength = AAsset_getLength(file);

    char *fileContent = new char[fileLength];

    AAsset_read(file, fileContent, fileLength);
    AAsset_close(file);

    VkShaderModuleCreateInfo shaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = fileLength,
            .pCode = (const uint32_t *) fileContent,
    };
    VkResult result = vkCreateShaderModule(
            device.device_, &shaderModuleCreateInfo, nullptr, shaderOut);
    assert(result == VK_SUCCESS);

    delete[] fileContent;

    return result;
}

// Create Graphics Pipeline
VkResult CreateGraphicsPipeline(void) {
    memset(&gfxPipeline, 0, sizeof(gfxPipeline));
    // Create pipeline layout (empty)
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
    };
    CALL_VK(vkCreatePipelineLayout(device.device_, &pipelineLayoutCreateInfo,
                                   nullptr, &gfxPipeline.layout_));

    VkShaderModule vertexShader, fragmentShader;
    loadShaderFromFile("shaders/tri.vert.spv", &vertexShader, VERTEX_SHADER);
    loadShaderFromFile("shaders/tri.frag.spv", &fragmentShader, FRAGMENT_SHADER);

    // Specify vertex and fragment shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2]{
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vertexShader,
                    .pName = "main",
                    .pSpecializationInfo = nullptr,
            },
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragmentShader,
                    .pName = "main",
                    .pSpecializationInfo = nullptr,
            }};

    VkViewport viewports{
            .x = 0,
            .y = 0,
            .width = (float) swapchain.displaySize_.width,
            .height = (float) swapchain.displaySize_.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
            .offset {.x = 0, .y = 0,},
            .extent = swapchain.displaySize_,
    };
    // Specify viewport info
    VkPipelineViewportStateCreateInfo viewportInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .viewportCount = 1,
            .pViewports = &viewports,
            .scissorCount = 1,
            .pScissors = &scissor,
    };

    // Specify multisample info
    VkSampleMask sampleMask = ~0u;
    VkPipelineMultisampleStateCreateInfo multisampleInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 0,
            .pSampleMask = &sampleMask,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
    };

    // Specify color blend state
    VkPipelineColorBlendAttachmentState attachmentStates{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &attachmentStates,
    };

    // Specify rasterizer info
    VkPipelineRasterizationStateCreateInfo rasterInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1,
    };

    // Specify input assembler state
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
    };

    // Specify vertex input state
    VkVertexInputBindingDescription vertex_input_bindings{
            .binding = 0,
            .stride = 3 * sizeof(float),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription vertex_input_attributes[1]{{
                                                                         .location = 0,
                                                                         .binding = 0,
                                                                         .format = VK_FORMAT_R32G32B32_SFLOAT,
                                                                         .offset = 0,
                                                                 }};
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertex_input_bindings,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = vertex_input_attributes,
    };

    // Create the pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,  // reserved, must be 0
            .initialDataSize = 0,
            .pInitialData = nullptr,
    };

    CALL_VK(vkCreatePipelineCache(device.device_, &pipelineCacheInfo, nullptr,
                                  &gfxPipeline.cache_));

    // Create the pipeline
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pTessellationState = nullptr,
            .pViewportState = &viewportInfo,
            .pRasterizationState = &rasterInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlendInfo,
            .pDynamicState = nullptr,
            .layout = gfxPipeline.layout_,
            .renderPass = render.renderPass_,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
    };

    VkResult pipelineResult = vkCreateGraphicsPipelines(
            device.device_, gfxPipeline.cache_, 1, &pipelineCreateInfo, nullptr,
            &gfxPipeline.pipeline_);

    // We don't need the shaders anymore, we can release their memory
    vkDestroyShaderModule(device.device_, vertexShader, nullptr);
    vkDestroyShaderModule(device.device_, fragmentShader, nullptr);

    return pipelineResult;
}

void DeleteGraphicsPipeline(void) {
    if (gfxPipeline.pipeline_ == VK_NULL_HANDLE) return;
    vkDestroyPipeline(device.device_, gfxPipeline.pipeline_, nullptr);
    vkDestroyPipelineCache(device.device_, gfxPipeline.cache_, nullptr);
    vkDestroyPipelineLayout(device.device_, gfxPipeline.layout_, nullptr);
}

// InitVulkan: Vulkan状态的初始化
//   Initialize Vulkan Context when android application window is created
//   upon return, vulkan is ready to draw frames
bool InitVulkan(android_app *app) {
    androidAppCtx = app;

    // 获取libvulkan.so中含有的vulkan函数
    if (!InitVulkan()) {
        LOGW("Vulkan is unavailable, install vulkan and re-start");
        return false;
    }

    // 依次创建vulkan全局数据结构
    device.instance_ = getInstance();
    device.surface_ = getSurface(device.instance_, app->window);
    device.gpuDevice_ = getPhysicalDevice(device.instance_, device.surface_);
    device.queueFamilyIndex_ = getQueueFamilyIndex(device.gpuDevice_);
    device.device_ = getDevice(device.gpuDevice_, device.queueFamilyIndex_);
    device.queue_ = getQueue(device.device_, device.queueFamilyIndex_);

    // 创建交换链
    getSwapChain(device.surface_, device.gpuDevice_, device.queueFamilyIndex_, device.device_, &swapchain);

    // 创建render pass
    render.renderPass_ = getRenderPass(device.device_, swapchain.displayFormat_);

    // 依次创建Image、imageView、FrameBuffer
    getFrameBuffers(device.device_, render.renderPass_, &swapchain);

    // 创建指令池
    render.cmdPool_ = getCommandPool(device.device_, device.queueFamilyIndex_);

    // 创建指令缓冲（为每一帧）
    getCommandBuffers(device.device_, swapchain.swapchainLength_, render.cmdPool_, &render);


// ============================ 以上为初始化完成，以下为每帧的信息（cmdpool可能可以初始化创建）============================


    // create vertex buffers
    CreateBuffers();

    // Create graphics pipeline
    CreateGraphicsPipeline();

    // -----------------------------------------------



    for (int bufferIndex = 0; bufferIndex < swapchain.swapchainLength_;
         bufferIndex++) {
        // We start by creating and declare the "beginning" our command buffer
        VkCommandBufferBeginInfo cmdBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = 0,
                .pInheritanceInfo = nullptr,
        };
        CALL_VK(vkBeginCommandBuffer(render.cmdBuffer_[bufferIndex],
                                     &cmdBufferBeginInfo));
        // transition the display image to color attachment layout
        setImageLayout(render.cmdBuffer_[bufferIndex],
                       swapchain.displayImages_[bufferIndex],
                       VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        // Now we start a renderpass. Any draw command has to be recorded in a
        // renderpass
        VkClearValue clearVals{.color {.float32 {0.0f, 0.9f, 0.90f, 1.0f}}};
        VkRenderPassBeginInfo renderPassBeginInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext = nullptr,
                .renderPass = render.renderPass_,
                .framebuffer = swapchain.framebuffers_[bufferIndex],
                .renderArea = {.offset {.x = 0, .y = 0,},
                        .extent = swapchain.displaySize_},
                .clearValueCount = 1,
                .pClearValues = &clearVals};
        vkCmdBeginRenderPass(render.cmdBuffer_[bufferIndex], &renderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);
        // Bind what is necessary to the command buffer
        vkCmdBindPipeline(render.cmdBuffer_[bufferIndex],
                          VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.pipeline_);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(render.cmdBuffer_[bufferIndex], 0, 1,
                               &buffers.vertexBuf_, &offset);

        // Draw Triangle
        vkCmdDraw(render.cmdBuffer_[bufferIndex], 3, 1, 0, 0);

        vkCmdEndRenderPass(render.cmdBuffer_[bufferIndex]);

        CALL_VK(vkEndCommandBuffer(render.cmdBuffer_[bufferIndex]));
    }

    // We need to create a fence to be able, in the main loop, to wait for our
    // draw command(s) to finish before swapping the framebuffers
    VkFenceCreateInfo fenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
    };
    CALL_VK(
            vkCreateFence(device.device_, &fenceCreateInfo, nullptr, &render.fence_));

    // We need to create a semaphore to be able to wait, in the main loop, for our
    // framebuffer to be available for us before drawing.
    VkSemaphoreCreateInfo semaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
    };
    CALL_VK(vkCreateSemaphore(device.device_, &semaphoreCreateInfo, nullptr,
                              &render.semaphore_));

    device.initialized_ = true;
    return true;
}

// IsVulkanReady():
//    native app poll to see if we are ready to draw...
bool IsVulkanReady(void) {
    return device.initialized_;
}

void DeleteVulkan(void) {
    vkFreeCommandBuffers(device.device_, render.cmdPool_, render.cmdBuffer_.size(),
                         render.cmdBuffer_.data());
    render.cmdBuffer_.clear();
    
    vkDestroyCommandPool(device.device_, render.cmdPool_, nullptr);
    vkDestroyRenderPass(device.device_, render.renderPass_, nullptr);
    DeleteSwapChain(device.device_, &swapchain);
    DeleteGraphicsPipeline();
    DeleteBuffers();

    vkDestroyDevice(device.device_, nullptr);
    vkDestroyInstance(device.instance_, nullptr);

    device.initialized_ = false;
}

// Draw one frame
bool VulkanDrawFrame(void) {
    uint32_t nextIndex;
    // Get the framebuffer index we should draw in
    CALL_VK(vkAcquireNextImageKHR(device.device_, swapchain.swapchain_,
                                  UINT64_MAX, render.semaphore_, VK_NULL_HANDLE,
                                  &nextIndex));
    CALL_VK(vkResetFences(device.device_, 1, &render.fence_));

    VkPipelineStageFlags waitStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render.semaphore_,
            .pWaitDstStageMask = &waitStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &render.cmdBuffer_[nextIndex],
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr};
    CALL_VK(vkQueueSubmit(device.queue_, 1, &submit_info, render.fence_));
    CALL_VK(
            vkWaitForFences(device.device_, 1, &render.fence_, VK_TRUE, 100000000));

    //LOGI("Drawing frames......");

    VkResult result;
    VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain_,
            .pImageIndices = &nextIndex,
            .pResults = &result,
    };
    vkQueuePresentKHR(device.queue_, &presentInfo);
    return true;
}

/*
 * setImageLayout():
 *    Helper function to transition color buffer layout
 */
void setImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
                    VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                    VkPipelineStageFlags srcStages,
                    VkPipelineStageFlags destStages) {
    VkImageMemoryBarrier imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = oldImageLayout,
            .newLayout = newImageLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange =
                    {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                    },
    };

    switch (oldImageLayout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        default:
            break;
    }

    switch (newImageLayout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.dstAccessMask =
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            break;

        default:
            break;
    }

    vkCmdPipelineBarrier(cmdBuffer, srcStages, destStages, 0, 0, NULL, 0, NULL, 1,
                         &imageMemoryBarrier);
}
