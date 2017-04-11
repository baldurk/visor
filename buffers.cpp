#include "precompiled.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo,
                                              const VkAllocationCallbacks *pAllocator,
                                              VkBuffer *pBuffer)
{
  VkBuffer ret = new VkBuffer_T;
  ret->size = pCreateInfo->size;
  ret->bytes = NULL;    // no memory bound
  *pBuffer = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice device, VkBuffer buffer,
                                           const VkAllocationCallbacks *pAllocator)
{
  delete buffer;
}

VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer,
                                                         VkMemoryRequirements *pMemoryRequirements)
{
  // TODO
  pMemoryRequirements->alignment = 1;
  pMemoryRequirements->memoryTypeBits = 0x3;
  pMemoryRequirements->size = buffer->size;
}

VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(VkDevice device, VkBuffer buffer,
                                                  VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
  buffer->bytes = memory->bytes + memoryOffset;
  return VK_SUCCESS;
}
