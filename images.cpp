#include "precompiled.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(VkDevice device,
                                                 const VkImageViewCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkImageView *pImageView)
{
  // TODO but for now return unique values
  static uint64_t nextImageView = 1;
  *pImageView = (VkImageView)(nextImageView++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice device, VkImageView imageView,
                                              const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
  VkImage ret = new VkImage_T;
  ret->pixels = NULL;    // no memory bound
  ret->extent = {pCreateInfo->extent.width, pCreateInfo->extent.height};
  *pImage = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice device, VkImage image,
                                          const VkAllocationCallbacks *pAllocator)
{
  delete image;
}

VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(VkDevice device, VkImage image,
                                                 VkDeviceMemory memory, VkDeviceSize memoryOffset)
{
  image->pixels = memory->bytes + memoryOffset;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(VkDevice device, VkImage image,
                                                        VkMemoryRequirements *pMemoryRequirements)
{
  // TODO
  pMemoryRequirements->alignment = 1;
  pMemoryRequirements->memoryTypeBits = 0x3;
  pMemoryRequirements->size = image->extent.width * image->extent.height * 4;
}

VKAPI_ATTR void VKAPI_CALL vkGetImageSubresourceLayout(VkDevice device, VkImage image,
                                                       const VkImageSubresource *pSubresource,
                                                       VkSubresourceLayout *pLayout)
{
  assert(pSubresource->arrayLayer == 0 && pSubresource->mipLevel == 0 &&
         pSubresource->aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);

  pLayout->offset = 0;
  pLayout->rowPitch = image->extent.width * 4;
  pLayout->arrayPitch = pLayout->depthPitch = pLayout->size =
      image->extent.width * image->extent.height * 4;
}
