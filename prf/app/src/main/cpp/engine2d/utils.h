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

#endif //PRF_ENGINE2D_UTILS_H
