#pragma once

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
  VkDeviceSize size;
  byte *bytes;
};

struct VkImage_T
{
  VkExtent2D extent;
  byte *pixels = NULL;
};

struct VkBuffer_T
{
  VkDeviceSize size;
};

struct VkSwapchainKHR_T
{
  VkExtent2D extent;

  struct Backbuffer
  {
    VkImage im;
    VkDeviceMemory mem;

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
