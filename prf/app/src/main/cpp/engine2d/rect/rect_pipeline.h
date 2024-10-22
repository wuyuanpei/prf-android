//
// Created by richardwu on 10/22/24.
//

#ifndef PRF_RECT_PIPELINE_H
#define PRF_RECT_PIPELINE_H

#include "../../vulkan/utils.h"
#include "../utils.h"
#include "../pipeline.h"

void createRectGraphicsPipeline(android_app *androidAppCtx, VkDevice device, VkExtent2D extent2D,
                            VkRenderPass renderPass, VkPipelineCache pipelineCache,
                            VulkanPipelineInfo *pipelineInfo) {
    createGraphicsPipeline(androidAppCtx, device, extent2D, renderPass, pipelineCache, pipelineInfo);
}

#endif //PRF_RECT_PIPELINE_H
