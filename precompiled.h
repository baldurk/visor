#pragma once

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR 1
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#endif

#include <assert.h>
#include <stdint.h>
#include <algorithm>
#include <vector>
#include "3rdparty/microprofile.h"
#include "3rdparty/vk_icd.h"
#include "3rdparty/vulkan.h"

typedef unsigned char byte;

struct VkDevice_T
{
  uintptr_t loaderMagic;
  std::vector<VK_LOADER_DATA *> queues;
};

struct VkCommandBuffer_T
{
  uintptr_t loaderMagic;
};

struct VkDeviceMemory_T
{
  VkDeviceSize size = 0;
  byte *bytes = NULL;
};

struct VkImage_T
{
  VkExtent2D extent = {0, 0};
  byte *pixels = NULL;
};

struct VkImageView_T
{
  VkImage_T *image = NULL;
};

struct VkBuffer_T
{
  VkDeviceSize size = 0;
};

struct VkSwapchainKHR_T
{
  VkExtent2D extent = {0, 0};

  struct Backbuffer
  {
    VkImage im = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;

#if defined(_WIN32)
    HDC dc = NULL;
    HBITMAP bmp = NULL;
#endif
  };

  std::vector<Backbuffer> backbuffers;

#if defined(_WIN32)
  HWND wnd = NULL;
  HDC dc = NULL;
#endif

  uint32_t current = 0;
};
