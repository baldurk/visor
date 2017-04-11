#include "precompiled.h"

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
