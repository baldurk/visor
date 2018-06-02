#include "precompiled.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(VkDevice device,
                                                 const VkImageViewCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator,
                                                 VkImageView *pImageView)
{
  VkImageView ret = new VkImageView_T;
  ret->image = pCreateInfo->image;
  *pImageView = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice device, VkImageView imageView,
                                              const VkAllocationCallbacks *pAllocator)
{
  delete imageView;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator, VkImage *pImage)
{
  VkImage ret = new VkImage_T;
  ret->pixels = NULL;    // no memory bound
  ret->extent = pCreateInfo->extent;
  ret->bytesPerPixel = 4;
  ret->imageType = pCreateInfo->imageType;
  ret->format = pCreateInfo->format;
  ret->arrayLayers = pCreateInfo->arrayLayers;
  ret->mipLevels = pCreateInfo->mipLevels;
  if(pCreateInfo->format == VK_FORMAT_R8_UNORM || pCreateInfo->format == VK_FORMAT_BC2_UNORM_BLOCK ||
     pCreateInfo->format == VK_FORMAT_BC3_UNORM_BLOCK)
    ret->bytesPerPixel = 1;
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
  pMemoryRequirements->size =
      image->extent.width * image->extent.height * image->arrayLayers * image->bytesPerPixel;
  if(image->imageType == VK_IMAGE_TYPE_3D)
    pMemoryRequirements->size *= image->extent.depth;

  // allocate a bunch more space for mips
  if(image->mipLevels > 1)
    pMemoryRequirements->size *= 2;
}

VKAPI_ATTR void VKAPI_CALL vkGetImageSubresourceLayout(VkDevice device, VkImage image,
                                                       const VkImageSubresource *pSubresource,
                                                       VkSubresourceLayout *pLayout)
{
  assert(pSubresource->arrayLayer == 0 && pSubresource->mipLevel == 0 &&
         pSubresource->aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);

  pLayout->offset = 0;
  pLayout->rowPitch = image->extent.width * image->bytesPerPixel;
  pLayout->arrayPitch = pLayout->depthPitch = pLayout->size =
      image->extent.width * image->extent.height * image->bytesPerPixel;
}
