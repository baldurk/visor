#include "icd_common.h"
#include "wsi.h"

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                                                    uint32_t queueFamilyIndex,
                                                                    VkSurfaceKHR surface,
                                                                    VkBool32 *pSupported)
{
  // support presenting on all queues
  if(pSupported)
    *pSupported = VK_TRUE;
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                                                    VkSurfaceKHR surface,
                                                                    uint32_t *pSurfaceFormatCount,
                                                                    VkSurfaceFormatKHR *pSurfaceFormats)
{
  // support BGRA8 in UNORM and SRGB modes
  if(pSurfaceFormatCount && !pSurfaceFormats)
  {
    *pSurfaceFormatCount = 2;
    return VK_SUCCESS;
  }

  pSurfaceFormats[0].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  pSurfaceFormats[0].format = VK_FORMAT_B8G8R8A8_SRGB;
  pSurfaceFormats[1].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  pSurfaceFormats[1].format = VK_FORMAT_B8G8R8A8_UNORM;

  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                          VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
  VkIcdSurfaceBase *base = (VkIcdSurfaceBase *)surface;
#if defined(_WIN32)
  if(base->platform == VK_ICD_WSI_PLATFORM_WIN32)
  {
    memset(pSurfaceCapabilities, 0, sizeof(VkSurfaceCapabilitiesKHR));

    VkIcdSurfaceWin32 *win32 = (VkIcdSurfaceWin32 *)base;

    pSurfaceCapabilities->minImageCount = 1;
    pSurfaceCapabilities->maxImageCount = 2;
    pSurfaceCapabilities->minImageExtent = {1, 1};
    pSurfaceCapabilities->maxImageExtent = {32768, 32768};
    pSurfaceCapabilities->maxImageArrayLayers = 1;

    RECT rect = {};
    GetClientRect(win32->hwnd, &rect);

    pSurfaceCapabilities->currentExtent = {uint32_t(rect.right - rect.left),
                                           uint32_t(rect.bottom - rect.top)};

    pSurfaceCapabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    pSurfaceCapabilities->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    pSurfaceCapabilities->supportedUsageFlags =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    return VK_SUCCESS;
  }
#endif
  return VK_ERROR_DEVICE_LOST;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t *pPresentModeCount,
    VkPresentModeKHR *pPresentModes)
{
  // only support FIFO
  if(pPresentModeCount && !pPresentModes)
  {
    *pPresentModeCount = 1;
    return VK_SUCCESS;
  }

  pPresentModes[0] = VK_PRESENT_MODE_FIFO_KHR;

  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device,
                                                    const VkSwapchainCreateInfoKHR *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkSwapchainKHR *pSwapchain)
{
  VkSwapchainKHR ret = new VkSwapchainKHR_T;

  // TODO probably want more properties out of here, although we restricted the options a lot
  VkIcdSurfaceBase *base = (VkIcdSurfaceBase *)pCreateInfo->surface;
#if defined(_WIN32)
  if(base->platform == VK_ICD_WSI_PLATFORM_WIN32)
  {
    VkIcdSurfaceWin32 *win32 = (VkIcdSurfaceWin32 *)base;
    ret->swap = CreateSwapchain(win32->hwnd, pCreateInfo->minImageCount);
  }
#endif
  assert(ret->swap);
  ret->images.resize(pCreateInfo->minImageCount);
  for(size_t i = 0; i < ret->images.size(); i++)
  {
    ret->images[i] = new VkImage_T();
    ret->images[i]->extent = pCreateInfo->imageExtent;
    ret->images[i]->pixels = GetImagePixels(ret->swap, (int)i);
  }
  ret->extent = pCreateInfo->imageExtent;

  *pSwapchain = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                 const VkAllocationCallbacks *pAllocator)
{
  delete swapchain;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                       uint32_t *pSwapchainImageCount,
                                                       VkImage *pSwapchainImages)
{
  if(pSwapchainImageCount && !pSwapchainImages)
  {
    *pSwapchainImageCount = (uint32_t)swapchain->images.size();
    return VK_SUCCESS;
  }

  for(uint32_t i = 0; i < *pSwapchainImageCount; i++)
    pSwapchainImages[i] = swapchain->images[i];

  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                     uint64_t timeout, VkSemaphore semaphore,
                                                     VkFence fence, uint32_t *pImageIndex)
{
  // ignore fence and semaphore
  *pImageIndex = (uint32_t)Acquire(swapchain->swap);

  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
  // TODO
  return VK_SUCCESS;
}
