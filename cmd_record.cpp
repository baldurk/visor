#include "precompiled.h"
#include "commands.h"

VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                    const VkCommandBufferBeginInfo *pBeginInfo)
{
  // TODO
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer commandBuffer)
{
  // TODO
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
  auto *cmd = commandBuffer->push<cmd::PipelineBarrier>();
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer commandBuffer,
                                                const VkRenderPassBeginInfo *pRenderPassBegin,
                                                VkSubpassContents contents)
{
  auto *cmd = commandBuffer->push<cmd::BeginRenderPass>();
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
  auto *cmd = commandBuffer->push<cmd::EndRenderPass>();
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                             VkPipelineBindPoint pipelineBindPoint,
                                             VkPipeline pipeline)
{
  auto *cmd = commandBuffer->push<cmd::BindPipeline>();
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
    uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets,
    uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets)
{
  auto *cmd = commandBuffer->push<cmd::BindDescriptorSets>();
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport,
                                            uint32_t viewportCount, const VkViewport *pViewports)
{
  auto *cmd = commandBuffer->push<cmd::SetViewport>();
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor,
                                           uint32_t scissorCount, const VkRect2D *pScissors)
{
  auto *cmd = commandBuffer->push<cmd::SetScissors>();
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount,
                                     uint32_t instanceCount, uint32_t firstVertex,
                                     uint32_t firstInstance)
{
  auto *cmd = commandBuffer->push<cmd::Draw>();
  // TODO
}
