#include "precompiled.h"
#include "commands.h"

VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                    const VkCommandBufferBeginInfo *pBeginInfo)
{
  commandBuffer->commandStream.clear();
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
  cmd::PipelineBarrier *cmd = commandBuffer->push<cmd::PipelineBarrier>();
}

VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer commandBuffer,
                                                const VkRenderPassBeginInfo *pRenderPassBegin,
                                                VkSubpassContents contents)
{
  cmd::BeginRenderPass *cmd = commandBuffer->push<cmd::BeginRenderPass>();
  cmd->renderPass = pRenderPassBegin->renderPass;
  cmd->framebuffer = pRenderPassBegin->framebuffer;

  memcpy(cmd->clearval, pRenderPassBegin->pClearValues,
         sizeof(VkClearValue) * std::min(pRenderPassBegin->clearValueCount, 8U));
}

VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
  cmd::EndRenderPass *cmd = commandBuffer->push<cmd::EndRenderPass>();
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                             VkPipelineBindPoint pipelineBindPoint,
                                             VkPipeline pipeline)
{
  cmd::BindPipeline *cmd = commandBuffer->push<cmd::BindPipeline>();
  cmd->pipeline = pipeline;
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
    uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets,
    uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets)
{
  assert(dynamicOffsetCount == 0);

  for(uint32_t i = 0; i < descriptorSetCount; i++)
  {
    cmd::BindDescriptorSets *cmd = commandBuffer->push<cmd::BindDescriptorSets>();

    cmd->idx = firstSet + i;
    cmd->set = pDescriptorSets[i];
  }
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer,
                                                  uint32_t firstBinding, uint32_t bindingCount,
                                                  const VkBuffer *pBuffers,
                                                  const VkDeviceSize *pOffsets)
{
  for(uint32_t i = 0; i < bindingCount; i++)
  {
    cmd::BindVB *cmd = commandBuffer->push<cmd::BindVB>();
    cmd->slot = firstBinding + i;
    cmd->buffer = pBuffers[i];
    cmd->offset = pOffsets[i];
  }
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                VkDeviceSize offset, VkIndexType indexType)
{
  cmd::BindIB *cmd = commandBuffer->push<cmd::BindIB>();
  cmd->buffer = buffer;
  cmd->offset = offset;
  cmd->indexType = indexType;
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport,
                                            uint32_t viewportCount, const VkViewport *pViewports)
{
  cmd::SetViewport *cmd = commandBuffer->push<cmd::SetViewport>();
  cmd->view = pViewports[0];
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor,
                                           uint32_t scissorCount, const VkRect2D *pScissors)
{
  cmd::SetScissors *cmd = commandBuffer->push<cmd::SetScissors>();
}

VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
                                              VkShaderStageFlags stageFlags, uint32_t offset,
                                              uint32_t size, const void *pValues)
{
  cmd::PushConstants *cmd = commandBuffer->push<cmd::PushConstants>();
  cmd->offset = offset;
  cmd->size = size;
  memcpy(cmd->values, pValues, size);
}

VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount,
                                     uint32_t instanceCount, uint32_t firstVertex,
                                     uint32_t firstInstance)
{
  cmd::Draw *cmd = commandBuffer->push<cmd::Draw>();
  cmd->vertexCount = vertexCount;
  cmd->instanceCount = instanceCount;
  cmd->firstVertex = firstVertex;
  cmd->firstInstance = firstInstance;
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount,
                                            uint32_t instanceCount, uint32_t firstIndex,
                                            int32_t vertexOffset, uint32_t firstInstance)
{
  cmd::DrawIndexed *cmd = commandBuffer->push<cmd::DrawIndexed>();
  cmd->indexCount = indexCount;
  cmd->instanceCount = instanceCount;
  cmd->firstIndex = firstIndex;
  cmd->vertexOffset = vertexOffset;
  cmd->firstInstance = firstInstance;
}

VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
                                                  VkImage dstImage, VkImageLayout dstImageLayout,
                                                  uint32_t regionCount,
                                                  const VkBufferImageCopy *pRegions)
{
  for(uint32_t r = 0; r < regionCount; r++)
  {
    cmd::CopyBuf2Img *cmd = commandBuffer->push<cmd::CopyBuf2Img>();
    cmd->srcBuffer = srcBuffer;
    cmd->dstImage = dstImage;
    cmd->region = pRegions[r];
  }
}

VKAPI_ATTR void VKAPI_CALL vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer,
                                           VkBuffer dstBuffer, uint32_t regionCount,
                                           const VkBufferCopy *pRegions)
{
  for(uint32_t r = 0; r < regionCount; r++)
  {
    cmd::CopyBuf *cmd = commandBuffer->push<cmd::CopyBuf>();
    cmd->srcBuffer = srcBuffer;
    cmd->dstBuffer = dstBuffer;
    cmd->region = pRegions[r];
  }
}