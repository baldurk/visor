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

  if(pRenderPassBegin->clearValueCount > 0)
    cmd->clearval = pRenderPassBegin->pClearValues[0];
  else
    cmd->clearval = {};
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
  cmd::BindDescriptorSets *cmd = commandBuffer->push<cmd::BindDescriptorSets>();

  if(descriptorSetCount > 0)
    cmd->set = pDescriptorSets[0];
  else
    cmd->set = VK_NULL_HANDLE;
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