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
#include "vulkan/sync_objects.h"
#include "vulkan/pipeline_cache.h"

#include "engine2d/utils.h"
#include "engine2d/pipeline.h"
#include "engine2d/image_layout.h"
#include "engine2d/BufferManager.h"

#include <vulkan_wrapper.h>

#include <cassert>
#include <cstring>
#include <vector>
#include <stdlib.h>

VulkanDeviceInfo deviceInfo;
VulkanSwapchainInfo swapchainInfo;
VulkanRenderInfo renderInfo;

VulkanPipelineInfo pipelineInfo; // TODO：假设现在只有一个pipeline

/* 管理系统全局的所有各类型的VkBuffer */
BufferManager *vertexBufferManager;
BufferManager *indexBufferManager;

/*
 * setImageLayout():
 *    Helper function to transition color buffer layout
 */
void setImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
                    VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                    VkPipelineStageFlags srcStages,
                    VkPipelineStageFlags destStages);

// InitVulkan: Vulkan状态的初始化
//   Initialize Vulkan Context when android application window is created
//   upon return, vulkan is ready to draw frames
bool InitVulkan(android_app *app) {

    // 获取libvulkan.so中含有的vulkan函数
    if (!InitVulkan()) {
        LOGE("Vulkan is unavailable, install vulkan and re-start");
        return false;
    }

    // 依次创建vulkan全局数据结构
    deviceInfo.instance_ = getInstance();
    deviceInfo.surface_ = getSurface(deviceInfo.instance_, app->window);
    deviceInfo.physicalDevice_ = getPhysicalDevice(deviceInfo.instance_, deviceInfo.surface_);
    deviceInfo.queueFamilyIndex_ = getQueueFamilyIndex(deviceInfo.physicalDevice_);
    deviceInfo.device_ = getDevice(deviceInfo.physicalDevice_, deviceInfo.queueFamilyIndex_);
    deviceInfo.queue_ = getQueue(deviceInfo.device_, deviceInfo.queueFamilyIndex_);

    // 创建交换链
    getSwapChain(deviceInfo.surface_, deviceInfo.physicalDevice_, deviceInfo.queueFamilyIndex_, deviceInfo.device_, &swapchainInfo);

    // 创建render pass
    renderInfo.renderPass_ = getRenderPass(deviceInfo.device_, swapchainInfo.displayFormat_);

    // 依次创建Image、imageView、FrameBuffer
    getFrameBuffers(deviceInfo.device_, renderInfo.renderPass_, &swapchainInfo);

    // 创建指令池
    renderInfo.cmdPool_ = getCommandPool(deviceInfo.device_, deviceInfo.queueFamilyIndex_);

    // 创建指令缓冲（为帧缓冲中的每一帧）
    getCommandBuffers(deviceInfo.device_, swapchainInfo.swapchainLength_, renderInfo.cmdPool_, &renderInfo);

    // 创建同步原语
    getImageAvailableSemaphore(deviceInfo.device_, &renderInfo);
    getRenderFinishedFence(deviceInfo.device_, &renderInfo);

    // 创建管线缓存
    renderInfo.pipelineCache_ = getPipelineCache(deviceInfo.device_);


// ============================ 以下为2d引擎资源管理器 ============================

    // 为每个2的整次幂维护一个可用VkBuffer的列表进行复用（全局数据结构）
    vertexBufferManager = new BufferManager(deviceInfo.device_, deviceInfo.physicalDevice_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indexBufferManager = new BufferManager(deviceInfo.device_, deviceInfo.physicalDevice_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // TODO: pipeline需要维护一个LRU的哈希表（全局数据结构）
    // Create graphics pipeline
    createGraphicsPipeline(app, deviceInfo.device_, swapchainInfo.displaySize_,
            renderInfo.renderPass_, renderInfo.pipelineCache_, &pipelineInfo);

    deviceInfo.initialized_ = true;
    return true;
}

// IsVulkanReady():
//    native app poll to see if we are ready to draw...
bool IsVulkanReady() {
    return deviceInfo.initialized_;
}

void DeleteVulkan() {

    vkDestroySemaphore(deviceInfo.device_, renderInfo.imageAvailableSemaphore_, nullptr);
    vkDestroyFence(deviceInfo.device_, renderInfo.renderFinishedFence_, nullptr);

    vkFreeCommandBuffers(deviceInfo.device_, renderInfo.cmdPool_, renderInfo.cmdBuffer_.size(),
                         renderInfo.cmdBuffer_.data());
    renderInfo.cmdBuffer_.clear();

    vkDestroyCommandPool(deviceInfo.device_, renderInfo.cmdPool_, nullptr);
    vkDestroyRenderPass(deviceInfo.device_, renderInfo.renderPass_, nullptr);
    DeleteSwapChain(deviceInfo.device_, &swapchainInfo);

    vkDestroyPipelineCache(deviceInfo.device_, renderInfo.pipelineCache_, nullptr);

    // TODO: 假设只有一个pipeline
    vkDestroyPipeline(deviceInfo.device_, pipelineInfo.pipeline_, nullptr);
    vkDestroyPipelineLayout(deviceInfo.device_, pipelineInfo.layout_, nullptr);

    // 调用析构函数，释放VkBuffer与VkDeviceMemory
    delete vertexBufferManager;
    delete indexBufferManager;

    vkDestroyDevice(deviceInfo.device_, nullptr);
    vkDestroyInstance(deviceInfo.instance_, nullptr);

    deviceInfo.initialized_ = false;
}

// Draw one frame
bool VulkanDrawFrame(android_app *app) {

    // 获取图片index
    uint32_t nextIndex;
    // Get the framebuffer index we should draw in
    CALL_VK(vkAcquireNextImageKHR(deviceInfo.device_, swapchainInfo.swapchain_,
                                  UINT64_MAX, renderInfo.imageAvailableSemaphore_, VK_NULL_HANDLE,
                                  &nextIndex));
    CALL_VK(vkResetFences(deviceInfo.device_, 1, &renderInfo.renderFinishedFence_));

    // 填写绘制命令
    // 首先，重置该帧在上次轮转时使用的资源
    vkResetCommandBuffer(renderInfo.cmdBuffer_[nextIndex], 0);
    vertexBufferManager->freeAllBuffers(nextIndex);
    indexBufferManager->freeAllBuffers(nextIndex);

//    vertexBufferManager->dump();
//    indexBufferManager->dump();

    // We create and declare the "beginning" our command buffer
    VkCommandBufferBeginInfo cmdBufferBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr,
    };
    CALL_VK(vkBeginCommandBuffer(renderInfo.cmdBuffer_[nextIndex],
                                 &cmdBufferBeginInfo));
//    // transition the display image to color attachment layout // TODO: 这里格式转换的必要性？
//    setImageLayout(renderInfo.cmdBuffer_[nextIndex],
//                   swapchainInfo.displayImages_[nextIndex],
//                   VK_IMAGE_LAYOUT_UNDEFINED,
//                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//                   VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
//                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    // Now we start a renderPass. Any draw command has to be recorded in a
    // renderPass
    VkClearValue clearVals = {{{1.0f, 1.0f, 1.0f, 0.0f}}};
    VkRenderPassBeginInfo renderPassBeginInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = renderInfo.renderPass_,
            .framebuffer = swapchainInfo.framebuffers_[nextIndex],
            .renderArea = {.offset {.x = 0, .y = 0,},
                    .extent = swapchainInfo.displaySize_},
            .clearValueCount = 1,
            .pClearValues = &clearVals};
    vkCmdBeginRenderPass(renderInfo.cmdBuffer_[nextIndex], &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    // Bind what is necessary to the command buffer
    vkCmdBindPipeline(renderInfo.cmdBuffer_[nextIndex],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineInfo.pipeline_);

    // 获取并填充VkBuffer////////////////////////TODO: 移入2d引擎中
    const float vertexData[] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5};
    VulkanBufferInfo vertexBufferInfo = vertexBufferManager->allocBuffer(nextIndex, sizeof(vertexData));
    void *data;
    vkMapMemory(deviceInfo.device_, vertexBufferInfo.bufferMemory_, 0, sizeof(vertexData),0, &data);
    memcpy(data, vertexData, sizeof(vertexData));
    vkUnmapMemory(deviceInfo.device_, vertexBufferInfo.bufferMemory_);
    ///////////////////////////////////////////////////////////////////////////
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(renderInfo.cmdBuffer_[nextIndex], 0, 1,
                           &vertexBufferInfo.buffer_, &offset);

    // 获取并填充VkBuffer////////////////////////TODO: 移入2d引擎中
    const uint16_t indexData[] = {0, 1, 2, 2, 3, 0};
    VulkanBufferInfo indexBufferInfo = indexBufferManager->allocBuffer(nextIndex, sizeof(indexData));
    vkMapMemory(deviceInfo.device_, indexBufferInfo.bufferMemory_, 0, sizeof(indexData),0, &data);
    memcpy(data, indexData, sizeof(indexData));
    vkUnmapMemory(deviceInfo.device_, indexBufferInfo.bufferMemory_);
    ///////////////////////////////////////////////////////////////////////////

    vkCmdBindIndexBuffer(renderInfo.cmdBuffer_[nextIndex], indexBufferInfo.buffer_, 0, VK_INDEX_TYPE_UINT16);
    // Draw Triangle
    vkCmdDrawIndexed(renderInfo.cmdBuffer_[nextIndex], 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(renderInfo.cmdBuffer_[nextIndex]);

    CALL_VK(vkEndCommandBuffer(renderInfo.cmdBuffer_[nextIndex]));


    // 提交指令
    VkPipelineStageFlags waitStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderInfo.imageAvailableSemaphore_,
            .pWaitDstStageMask = &waitStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &renderInfo.cmdBuffer_[nextIndex],
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr};
    CALL_VK(vkQueueSubmit(deviceInfo.queue_, 1, &submit_info, renderInfo.renderFinishedFence_));
    // Wait timeout set to 1 second
    CALL_VK(vkWaitForFences(deviceInfo.device_, 1, &renderInfo.renderFinishedFence_, VK_TRUE, 1000000000));

    // 递交显示
    VkResult result;
    VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .swapchainCount = 1,
            .pSwapchains = &swapchainInfo.swapchain_,
            .pImageIndices = &nextIndex,
            .pResults = &result,
    };
    vkQueuePresentKHR(deviceInfo.queue_, &presentInfo);
    return true;
}
