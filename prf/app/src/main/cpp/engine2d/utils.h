//
// Created by richardwu on 10/21/24.
//

#ifndef PRF_ENGINE2D_UTILS_H
#define PRF_ENGINE2D_UTILS_H

#include <vulkan_wrapper.h>

// 渲染管线信息
struct VulkanPipelineInfo {
    VkPipelineLayout layout_;
    VkPipeline pipeline_;
};

// 缓冲管理信息
struct VulkanBufferInfo {
    VkBuffer buffer_;
    VkDeviceMemory bufferMemory_;
};

#endif //PRF_ENGINE2D_UTILS_H
