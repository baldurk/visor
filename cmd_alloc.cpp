#include "precompiled.h"

VkCommandBuffer VkCommandPool_T::alloc()
{
  // brain dead algorithm
  for(VkCommandBuffer c : buffers)
  {
    if(!c->live)
    {
      c->live = true;
      return c;
    }
  }

  VkCommandBuffer ret = new VkCommandBuffer_T;
  ret->live = true;
  buffers.push_back(ret);
  return ret;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device,
                                                   const VkCommandPoolCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkCommandPool *pCommandPool)
{
  *pCommandPool = new VkCommandPool_T;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool,
                                                const VkAllocationCallbacks *pAllocator)
{
  for(VkCommandBuffer c : commandPool->buffers)
    delete c;
  delete commandPool;
}

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice device,
                                                        const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                        VkCommandBuffer *pCommandBuffers)
{
  for(uint32_t i = 0; i < pAllocateInfo->commandBufferCount; i++)
  {
    VkCommandBuffer cmd = pAllocateInfo->commandPool->alloc();
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
    pCommandBuffers[i]->live = false;
}

byte *VkCommandBuffer_T::pushbytes(size_t sz)
{
  size_t spare = commandStream.capacity() - commandStream.size();
  // if there's no spare capacity, allocate more
  if(sz > spare)
    commandStream.reserve(commandStream.capacity() * 2 + sz);

  // resize up to the newly used bytes, then return
  byte *ret = commandStream.data() + commandStream.size();
  commandStream.resize(commandStream.size() + sz);
  return ret;
}