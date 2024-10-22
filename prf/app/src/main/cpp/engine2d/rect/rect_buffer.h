//
// Created by richardwu on 10/22/24.
//

#ifndef PRF_RECT_BUFFER_H
#define PRF_RECT_BUFFER_H

#include "../../vulkan/utils.h"
#include "../utils.h"
#include "../buffer.h"

void createRectVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VulkanBufferInfo *vertexBufferInfo) {
    createVertexBuffer(device, physicalDevice, vertexBufferInfo);
}


void createRectIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VulkanBufferInfo *indexBufferInfo) {
    createIndexBuffer(device, physicalDevice, indexBufferInfo);
}

#endif //PRF_RECT_BUFFER_H
