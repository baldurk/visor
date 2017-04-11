#include "precompiled.h"

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
