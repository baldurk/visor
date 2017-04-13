#include "precompiled.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkInstance *pInstance)
{
  *pInstance = (VkInstance) new VK_LOADER_DATA;
  set_loader_magic_value(*pInstance);
  InitFrameStats();
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance instance,
                                             const VkAllocationCallbacks *pAllocator)
{
  ShutdownFrameStats();
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
