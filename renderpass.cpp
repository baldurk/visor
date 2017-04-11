#include "precompiled.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device,
                                                   const VkFramebufferCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator,
                                                   VkFramebuffer *pFramebuffer)
{
  VkFramebuffer ret = new VkFramebuffer_T;
  if(pCreateInfo->attachmentCount > 0)
    ret->attachments.insert(ret->attachments.begin(), pCreateInfo->pAttachments,
                            pCreateInfo->pAttachments + pCreateInfo->attachmentCount);
  *pFramebuffer = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer,
                                                const VkAllocationCallbacks *pAllocator)
{
  delete framebuffer;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device,
                                                  const VkRenderPassCreateInfo *pCreateInfo,
                                                  const VkAllocationCallbacks *pAllocator,
                                                  VkRenderPass *pRenderPass)
{
  VkRenderPass ret = new VkRenderPass_T;

  ret->subpasses.reserve(pCreateInfo->subpassCount);
  for(uint32_t i = 0; i < pCreateInfo->subpassCount; i++)
  {
    VkRenderPass_T::Subpass sub;
    sub.colAttachments.resize(pCreateInfo->pSubpasses[i].colorAttachmentCount);
    for(uint32_t a = 0; a < pCreateInfo->pSubpasses[i].colorAttachmentCount; a++)
    {
      sub.colAttachments[a].idx = pCreateInfo->pSubpasses[i].pColorAttachments[a].attachment;
      sub.colAttachments[a].clear = (pCreateInfo->pAttachments[sub.colAttachments[a].idx].loadOp ==
                                     VK_ATTACHMENT_LOAD_OP_CLEAR);
    }
    ret->subpasses.emplace_back(sub);
  }

  *pRenderPass = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass,
                                               const VkAllocationCallbacks *pAllocator)
{
  delete renderPass;
}
