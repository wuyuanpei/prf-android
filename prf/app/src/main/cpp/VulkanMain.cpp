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
#include "engine2d/vertex_buffer.h"

#include <vulkan_wrapper.h>

#include <cassert>
#include <cstring>
#include <vector>

VulkanDeviceInfo deviceInfo;
VulkanSwapchainInfo swapchainInfo;
VulkanRenderInfo renderInfo;

VulkanPipelineInfo pipelineInfo; // TODO：假设现在只有一个pipeline
VkBuffer vertexBuffer; // TODO：假设现在只有一个buffer

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
        LOGW("Vulkan is unavailable, install vulkan and re-start");
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


// ============================ 以上为初始化完成，以下为每帧的信息 ============================


    // create vertex buffers
    vertexBuffer = createVertexBuffer(deviceInfo.device_, deviceInfo.physicalDevice_, deviceInfo.queueFamilyIndex_);

    // TODO: pipeline需要维护一个LRU的哈希表
    // Create graphics pipeline
    createGraphicsPipeline(app, deviceInfo.device_, swapchainInfo.displaySize_,
            renderInfo.renderPass_, renderInfo.pipelineCache_, &pipelineInfo);

    // -----------------------------------------------


    // TODO: 首先将下面的代码移植到每帧中，其次将vertex buffers，graphics pipeline考虑移植
    for (int bufferIndex = 0; bufferIndex < swapchainInfo.swapchainLength_;
         bufferIndex++) {
        // We start by creating and declare the "beginning" our command buffer
        VkCommandBufferBeginInfo cmdBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = 0,
                .pInheritanceInfo = nullptr,
        };
        CALL_VK(vkBeginCommandBuffer(renderInfo.cmdBuffer_[bufferIndex],
                                     &cmdBufferBeginInfo));
        // transition the display image to color attachment layout
        setImageLayout(renderInfo.cmdBuffer_[bufferIndex],
                       swapchainInfo.displayImages_[bufferIndex],
                       VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        // Now we start a renderPass. Any draw command has to be recorded in a
        // renderPass
        VkClearValue clearVals{.color {.float32 {0.0f, 0.9f, 0.90f, 1.0f}}};
        VkRenderPassBeginInfo renderPassBeginInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext = nullptr,
                .renderPass = renderInfo.renderPass_,
                .framebuffer = swapchainInfo.framebuffers_[bufferIndex],
                .renderArea = {.offset {.x = 0, .y = 0,},
                        .extent = swapchainInfo.displaySize_},
                .clearValueCount = 1,
                .pClearValues = &clearVals};
        vkCmdBeginRenderPass(renderInfo.cmdBuffer_[bufferIndex], &renderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);
        // Bind what is necessary to the command buffer
        vkCmdBindPipeline(renderInfo.cmdBuffer_[bufferIndex],
                          VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineInfo.pipeline_);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(renderInfo.cmdBuffer_[bufferIndex], 0, 1,
                               &vertexBuffer, &offset);

        // Draw Triangle
        vkCmdDraw(renderInfo.cmdBuffer_[bufferIndex], 3, 1, 0, 0);

        vkCmdEndRenderPass(renderInfo.cmdBuffer_[bufferIndex]);

        CALL_VK(vkEndCommandBuffer(renderInfo.cmdBuffer_[bufferIndex]));
    }

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

    // TODO: 假设只有一个vertexBuffer
    vkDestroyBuffer(deviceInfo.device_, vertexBuffer, nullptr);

    vkDestroyDevice(deviceInfo.device_, nullptr);
    vkDestroyInstance(deviceInfo.instance_, nullptr);

    deviceInfo.initialized_ = false;
}

// Draw one frame
bool VulkanDrawFrame(android_app *app) {
    uint32_t nextIndex;
    // Get the framebuffer index we should draw in
    CALL_VK(vkAcquireNextImageKHR(deviceInfo.device_, swapchainInfo.swapchain_,
                                  UINT64_MAX, renderInfo.imageAvailableSemaphore_, VK_NULL_HANDLE,
                                  &nextIndex));
    CALL_VK(vkResetFences(deviceInfo.device_, 1, &renderInfo.renderFinishedFence_));

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

    // 绘制超时被设置为1秒
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
            .pNext = nullptr,
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

    vkCmdPipelineBarrier(cmdBuffer, srcStages, destStages, 0, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrier);
}
