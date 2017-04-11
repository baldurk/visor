#include "precompiled.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkInstance *pInstance)
{
  *pInstance = (VkInstance) new VK_LOADER_DATA;
  set_loader_magic_value(*pInstance);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance instance,
                                             const VkAllocationCallbacks *pAllocator)
{
  delete(VK_LOADER_DATA *)instance;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice,
                                              const VkDeviceCreateInfo *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator,
                                              VkDevice *pDevice)
{
  VkDevice dev = new VkDevice_T;
  assert((void *)dev == &dev->loaderMagic);
  set_loader_magic_value(dev);

  // we only have 1 queue family, there should only be 1 info
  assert(pCreateInfo->queueCreateInfoCount == 1);
  dev->queues.resize(pCreateInfo->pQueueCreateInfos[0].queueCount);

  for(uint32_t i = 0; i < pCreateInfo->pQueueCreateInfos[0].queueCount; i++)
  {
    dev->queues[i] = new VK_LOADER_DATA;
    set_loader_magic_value(dev->queues[i]);
  }

  *pDevice = dev;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{
  for(VK_LOADER_DATA *q : device->queues)
    delete q;

  delete device;
}

VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex,
                                            uint32_t queueIndex, VkQueue *pQueue)
{
  assert(queueFamilyIndex == 0);

  *pQueue = (VkQueue)device->queues[queueIndex];
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice device, const VkFenceCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator, VkFence *pFence)
{
  // TODO but for now return unique values
  static uint64_t nextFence = 1;
  *pFence = (VkFence)(nextFence++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice device, VkFence fence,
                                          const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(VkDevice device,
                                                 const VkSemaphoreCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkSemaphore *pSemaphore)
{
  // TODO but for now return unique values
  static uint64_t nextSemaphore = 1;
  *pSemaphore = (VkSemaphore)(nextSemaphore++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(VkDevice device, VkSemaphore semaphore,
                                              const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device,
                                                   const VkCommandPoolCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkCommandPool *pCommandPool)
{
  // TODO but for now return unique values
  static uint64_t nextCommandPool = 1;
  *pCommandPool = (VkCommandPool)(nextCommandPool++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool,
                                                const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device,
                                                        const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                        VkCommandBuffer *pCommandBuffers)
{
  for(uint32_t i = 0; i < pAllocateInfo->commandBufferCount; i++)
  {
    VkCommandBuffer cmd = new VkCommandBuffer_T;
    set_loader_magic_value(cmd);
    pCommandBuffers[i] = cmd;
  }
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
                                                uint32_t commandBufferCount,
                                                const VkCommandBuffer *pCommandBuffers)
{
  for(uint32_t i = 0; i < commandBufferCount; i++)
    delete pCommandBuffers[i];
}

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

VKAPI_ATTR VkResult VKAPI_CALL vkCreateSampler(VkDevice device,
                                               const VkSamplerCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator,
                                               VkSampler *pSampler)
{
  // TODO but for now return unique values
  static uint64_t nextSampler = 1;
  *pSampler = (VkSampler)(nextSampler++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroySampler(VkDevice device, VkSampler sampler,
                                            const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pDescriptorSetLayout)
{
  // TODO but for now return unique values
  static uint64_t nextDescriptorSetLayout = 1;
  *pDescriptorSetLayout = (VkDescriptorSetLayout)(nextDescriptorSetLayout++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device,
                                                        VkDescriptorSetLayout descriptorSetLayout,
                                                        const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device,
                                                      const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator,
                                                      VkPipelineLayout *pPipelineLayout)
{
  // TODO but for now return unique values
  static uint64_t nextPipelineLayout = 1;
  *pPipelineLayout = (VkPipelineLayout)(nextPipelineLayout++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout,
                                                   const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device,
                                                   const VkFramebufferCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkFramebuffer *pFramebuffer)
{
  // TODO but for now return unique values
  static uint64_t nextFramebuffer = 1;
  *pFramebuffer = (VkFramebuffer)(nextFramebuffer++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer,
                                                const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device,
                                                  const VkRenderPassCreateInfo *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkRenderPass *pRenderPass)
{
  // TODO but for now return unique values
  static uint64_t nextRenderPass = 1;
  *pRenderPass = (VkRenderPass)(nextRenderPass++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass,
                                               const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device,
                                                    const VkShaderModuleCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkShaderModule *pShaderModule)
{
  // TODO but for now return unique values
  static uint64_t nextShaderModule = 1;
  *pShaderModule = (VkShaderModule)(nextShaderModule++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule,
                                                 const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineCache(VkDevice device,
                                                     const VkPipelineCacheCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkPipelineCache *pPipelineCache)
{
  // TODO but for now return unique values
  static uint64_t nextPipelineCache = 1;
  *pPipelineCache = (VkPipelineCache)(nextPipelineCache++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache,
                                                  const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
                          const VkGraphicsPipelineCreateInfo *pCreateInfos,
                          const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
  // TODO but for now return unique values
  static uint64_t nextGraphicsPipeline = 1;
  for(uint32_t i = 0; i < createInfoCount; i++)
    pPipelines[i] = (VkPipeline)(nextGraphicsPipeline++);
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
                         const VkComputePipelineCreateInfo *pCreateInfos,
                         const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
  // TODO but for now return unique values
  static uint64_t nextComputePipeline = 1;
  for(uint32_t i = 0; i < createInfoCount; i++)
    pPipelines[i] = (VkPipeline)(nextComputePipeline++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline,
                                             const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device,
                                                      const VkDescriptorPoolCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator,
                                                      VkDescriptorPool *pDescriptorPool)
{
  // TODO but for now return unique values
  static uint64_t nextDescriptorPool = 1;
  *pDescriptorPool = (VkDescriptorPool)(nextDescriptorPool++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool sampler,
                                                   const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device,
                                                        const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                        VkDescriptorSet *pDescriptorSets)
{
  // TODO but for now return unique values
  static uint64_t nextDescriptorSet = 1;
  for(uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; i++)
    pDescriptorSets[0] = (VkDescriptorSet)(nextDescriptorSet++);
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool,
                                                    uint32_t descriptorSetCount,
                                                    const VkDescriptorSet *pDescriptorSets)
{
  // nothing to do
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount,
                                                  const VkWriteDescriptorSet *pDescriptorWrites,
                                                  uint32_t descriptorCopyCount,
                                                  const VkCopyDescriptorSet *pDescriptorCopies)
{
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers)
{
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer commandBuffer,
                                                const VkRenderPassBeginInfo *pRenderPassBegin,
                                                VkSubpassContents contents)
{
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                             VkPipelineBindPoint pipelineBindPoint,
                                             VkPipeline pipeline)
{
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
    uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets,
    uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets)
{
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport,
                                            uint32_t viewportCount, const VkViewport *pViewports)
{
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor,
                                           uint32_t scissorCount, const VkRect2D *pScissors)
{
  // TODO
}

VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount,
                                     uint32_t instanceCount, uint32_t firstVertex,
                                     uint32_t firstInstance)
{
  // TODO
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount,
                                             const VkSubmitInfo *pSubmits, VkFence fence)
{
  // TODO
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice device, uint32_t fenceCount,
                                               const VkFence *pFences, VkBool32 waitAll,
                                               uint64_t timeout)
{
  // TODO
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice device, uint32_t fenceCount,
                                             const VkFence *pFences)
{
  // TODO
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device)
{
  // TODO
  return VK_SUCCESS;
}
