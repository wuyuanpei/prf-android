//
// Created by richardwu on 10/21/24.
//

#ifndef PRF_VERTEX_BUFFER_H
#define PRF_VERTEX_BUFFER_H

#include <vulkan_wrapper.h>
#include "../vulkan/utils.h"

// A helper function
bool MapMemoryTypeToIndex(VkPhysicalDevice physicalDevice, uint32_t typeBits,
                          VkFlags requirements_mask, uint32_t *typeIndex) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    // Search mem types to find first index with those properties
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
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


VkBuffer createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex) {
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
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE, // 只被一个队列族拥有
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queueFamilyIndex,
    };

    VkBuffer vertexBuffer;
    CALL_VK(vkCreateBuffer(device, &createBufferInfo, nullptr, &vertexBuffer));

    /* 缓冲创建后还需要分配显存 */
    // 获取内存需求
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReq.size,
            .memoryTypeIndex = 0,  // Memory type assigned in the next step
    };

    // Assign the proper memory type for that buffer
    // 该段显存是CPU可见的，且coherent(显存和内存数据始终一致，避免cpu cache带来的问题)
    MapMemoryTypeToIndex(physicalDevice,
                         memReq.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         &allocInfo.memoryTypeIndex);

    // Allocate memory for the buffer
    VkDeviceMemory deviceMemory;
    CALL_VK(vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory));

    void *data;
    CALL_VK(vkMapMemory(device, deviceMemory, 0, allocInfo.allocationSize,0, &data));
    memcpy(data, vertexData, sizeof(vertexData));
    vkUnmapMemory(device, deviceMemory);

    CALL_VK(vkBindBufferMemory(device, vertexBuffer, deviceMemory, 0));
    return vertexBuffer;
}

#endif //PRF_VERTEX_BUFFER_H
