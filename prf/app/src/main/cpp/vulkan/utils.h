//
// Created by richardwu on 2024/10/18.
//

#ifndef PRF_UTILS_H
#define PRF_UTILS_H

#include <vulkan_wrapper.h>
#include "../log.h"

#include <cassert>
#include <vector>

// Vulkan调用封装
#define CALL_VK(func)                                                 \
  if (VK_SUCCESS != (func)) {                                         \
    __android_log_print(ANDROID_LOG_ERROR,                            \
                        "Vulkan error. File[%s], line[%d]", __FILE__, \
                        __LINE__);                                    \
    assert(false);                                                    \
  }

// Vulkan设备信息
struct VulkanDeviceInfo {
    bool initialized_;

    VkInstance instance_;
    VkPhysicalDevice gpuDevice_;
    VkDevice device_;
    uint32_t queueFamilyIndex_;

    VkSurfaceKHR surface_;
    VkQueue queue_;
};

// Vulkan交换链信息
struct VulkanSwapchainInfo {
    VkSwapchainKHR swapchain_;
    uint32_t swapchainLength_;

    VkExtent2D displaySize_;
    VkFormat displayFormat_;

    // array of frame buffers and views
    std::vector<VkImage> displayImages_;
    std::vector<VkImageView> displayViews_;
    std::vector<VkFramebuffer> framebuffers_;
};

#endif //PRF_UTILS_H
