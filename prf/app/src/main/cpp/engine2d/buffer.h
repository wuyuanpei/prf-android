//
// Created by richardwu on 10/21/24.
//

#ifndef PRF_BUFFER_H
#define PRF_BUFFER_H

#include <vulkan_wrapper.h>
#include "../vulkan/utils.h"

// A helper function
static bool mapMemoryTypeToIndex(VkPhysicalDevice physicalDevice, uint32_t typeBits,
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

/**
* 创建缓冲的辅助函数
* 我们想要在使用vertex buffer的同时使用暂存缓冲提升性能
* 可以使用不同的大小、usage、properties
*/
static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    CALL_VK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    mapMemoryTypeToIndex(physicalDevice, memRequirements.memoryTypeBits, properties, &allocInfo.memoryTypeIndex);

    CALL_VK(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory));

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}


void createVertexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VulkanBufferInfo *vertexBufferInfo) {
    // -----------------------------------------------
    // Create the triangle vertex buffer
    const float vertexData[] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5};

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexDeviceMemory;
    createBuffer(device, physicalDevice, sizeof(vertexData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 vertexBuffer, vertexDeviceMemory);

    void *data;
    vkMapMemory(device, vertexDeviceMemory, 0, sizeof(vertexData),0, &data);
    memcpy(data, vertexData, sizeof(vertexData));
    vkUnmapMemory(device, vertexDeviceMemory);

    vertexBufferInfo->buffer_ = vertexBuffer;
    vertexBufferInfo->bufferMemory_ = vertexDeviceMemory;
}

void createIndexBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VulkanBufferInfo *indexBufferInfo) {
    // -----------------------------------------------
    // Create the triangle vertex buffer

    // Vertex positions
    const uint16_t indexData[] = {0, 1, 2, 2, 3, 0};

    VkBuffer indexBuffer;
    VkDeviceMemory indexDeviceMemory;
    createBuffer(device, physicalDevice, sizeof(indexData), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 indexBuffer, indexDeviceMemory);

    void *data;
    vkMapMemory(device, indexDeviceMemory, 0, sizeof(indexData),0, &data);
    memcpy(data, indexData, sizeof(indexData));
    vkUnmapMemory(device, indexDeviceMemory);

    indexBufferInfo->buffer_ = indexBuffer;
    indexBufferInfo->bufferMemory_ = indexDeviceMemory;
}

#endif //PRF_BUFFER_H
