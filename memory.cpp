#include "precompiled.h"

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(VkDevice device,
                                                const VkMemoryAllocateInfo *pAllocateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkDeviceMemory *pMemory)
{
  VkDeviceMemory ret = new VkDeviceMemory_T;
  ret->size = pAllocateInfo->allocationSize;
  ret->bytes = new byte[pAllocateInfo->allocationSize];
  *pMemory = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                                        const VkAllocationCallbacks *pAllocator)
{
  delete[] memory->bytes;
  delete memory;
}

VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(VkDevice device, VkDeviceMemory memory,
                                           VkDeviceSize offset, VkDeviceSize size,
                                           VkMemoryMapFlags flags, void **ppData)
{
  byte *data = memory->bytes;
  data += offset;
  *ppData = (void *)data;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice device, VkDeviceMemory memory)
{
}