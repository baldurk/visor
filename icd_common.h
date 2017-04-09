#pragma once

#include "graphics.h"

#include "3rdparty/vk_icd.h"
#include "3rdparty/vulkan.h"

struct VkDevice_T
{
  ~VkDevice_T()
  {
    for(VK_LOADER_DATA *q : queues)
      delete q;
  }

  uintptr_t loaderMagic;
  std::vector<VK_LOADER_DATA *> queues;
};

struct VkCommandBuffer_T
{
  uintptr_t loaderMagic;
};

struct VkDeviceMemory_T
{
  byte *bytes;
};

struct VkImage_T
{
  ~VkImage_T() { Destroy(im); }
  Image *im;
  VkExtent2D extent;
};

struct VkBuffer_T
{
  VkDeviceSize size;
};

struct VkSwapchainKHR_T
{
  ~VkSwapchainKHR_T()
  {
    for(VkImage i : images)
    {
      // owned by the swapchain, destroyed below
      i->im = NULL;
      delete i;
    }
    Destroy(swap);
  }
  VkExtent2D extent;
  Swapchain *swap;
  std::vector<VkImage> images;
};
