#include "icd_common.h"

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

  ret->backbuffers.resize(pCreateInfo->minImageCount);

  ret->extent = pCreateInfo->imageExtent;

#if defined(_WIN32)
  if(base->platform == VK_ICD_WSI_PLATFORM_WIN32)
  {
    VkIcdSurfaceWin32 *win32 = (VkIcdSurfaceWin32 *)base;
    ret->wnd = win32->hwnd;
    ret->dc = GetDC(ret->wnd);

    for(size_t i = 0; i < ret->backbuffers.size(); i++)
    {
      ret->backbuffers[i].mem = new VkDeviceMemory_T();
      ret->backbuffers[i].mem->size = ret->extent.width * ret->extent.height * 4;

      HDC dc = CreateCompatibleDC(ret->dc);
      HBITMAP bmp = NULL;

      BITMAPINFO info = {};
      info.bmiHeader.biSize = sizeof(info.bmiHeader);
      info.bmiHeader.biWidth = ret->extent.width;
      info.bmiHeader.biHeight = ret->extent.height;
      info.bmiHeader.biPlanes = 1;
      info.bmiHeader.biBitCount = 32;
      info.bmiHeader.biCompression = BI_RGB;
      info.bmiHeader.biSizeImage = 0;
      info.bmiHeader.biXPelsPerMeter = info.bmiHeader.biYPelsPerMeter = 96;
      info.bmiHeader.biClrUsed = 0;
      info.bmiHeader.biClrImportant = 0;

      bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&ret->backbuffers[i].mem->bytes,
                             NULL, 0);
      assert(bmp && ret->backbuffers[i].mem->bytes);

      SelectObject(dc, bmp);

      ret->backbuffers[i].dc = dc;
      ret->backbuffers[i].bmp = bmp;
    }
  }
#endif

  for(size_t i = 0; i < ret->backbuffers.size(); i++)
  {
    ret->backbuffers[i].im = new VkImage_T();
    ret->backbuffers[i].im->extent = ret->extent;
    ret->backbuffers[i].im->pixels = ret->backbuffers[i].mem->bytes;
  }

  *pSwapchain = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                 const VkAllocationCallbacks *pAllocator)
{
  for(VkSwapchainKHR_T::Backbuffer &b : swapchain->backbuffers)
  {
    delete b.im;
    delete b.mem;

#if defined(_WIN32)
    if(b.dc)
      DeleteDC(b.dc);

    if(b.bmp)
      DeleteObject(b.bmp);
#endif
  }

  if(swapchain->dc)
    ReleaseDC(swapchain->wnd, swapchain->dc);

  delete swapchain;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                       uint32_t *pSwapchainImageCount,
                                                       VkImage *pSwapchainImages)
{
  if(pSwapchainImageCount && !pSwapchainImages)
  {
    *pSwapchainImageCount = (uint32_t)swapchain->backbuffers.size();
    return VK_SUCCESS;
  }

  for(uint32_t i = 0; i < *pSwapchainImageCount; i++)
    pSwapchainImages[i] = swapchain->backbuffers[i].im;

  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                     uint64_t timeout, VkSemaphore semaphore,
                                                     VkFence fence, uint32_t *pImageIndex)
{
  // ignore fence and semaphore
  swapchain->current = (swapchain->current + 1) % swapchain->backbuffers.size();
  *pImageIndex = swapchain->current;

  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
  for(uint32_t i = 0; i < pPresentInfo->swapchainCount; i++)
  {
    const VkSwapchainKHR &swap = pPresentInfo->pSwapchains[i];

    const VkSwapchainKHR_T::Backbuffer &bb = swap->backbuffers[pPresentInfo->pImageIndices[i]];

    BitBlt(swap->dc, 0, 0, swap->extent.width, swap->extent.height, bb.dc, 0, 0, SRCCOPY);
  }
  return VK_SUCCESS;
}
