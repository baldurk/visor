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

// see commands.h
enum class Command : uint16_t
{
  PipelineBarrier,
  BeginRenderPass,
  EndRenderPass,
  BindPipeline,
  BindDescriptorSets,
  BindVB,
  BindIB,
  SetViewport,
  SetScissors,
  Draw,
};

struct GPUState;
struct VertexCacheEntry;
struct float4;

typedef void (*Shader)();
typedef void (*VertexShader)(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out);
typedef void (*FragmentShader)(const GPUState &state, float pixdepth, const float4 &bary,
                               const VertexCacheEntry tri[3], float4 &out);

void InitPremadeShaders();
Shader GetPremadeShader(uint32_t hash);

struct VkCommandBuffer_T
{
  uintptr_t loaderMagic;
  bool live = false;
  std::vector<byte> commandStream;

  template <typename T>
  T *push()
  {
    Command *id = (Command *)pushbytes(sizeof(Command));
    *id = T::CommandID;
    return (T *)pushbytes(sizeof(T));
  }

  void execute() const;

private:
  byte *pushbytes(size_t sz);
};

struct VkCommandPool_T
{
  std::vector<VkCommandBuffer> buffers;
  VkCommandBuffer alloc();
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
  byte *bytes = NULL;
};

struct VkShaderModule_T
{
  Shader func = NULL;
};

struct VkPipeline_T
{
  VertexShader vs = NULL;
  FragmentShader fs = NULL;
};

struct VkDescriptorSetLayout_T
{
  uint32_t bindingCount;
};

struct VkDescriptorSet_T
{
  struct Bind
  {
    Bind() { memset(&data, 0, sizeof(data)); }
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;

    union
    {
      VkDescriptorImageInfo imageInfo;
      VkDescriptorBufferInfo bufferInfo;
      VkBufferView texelBufferView;
    } data;
  };

  std::vector<Bind> binds;
};

struct VkRenderPass_T
{
  struct Attachment
  {
    uint32_t idx;
    bool clear;
  };

  struct Subpass
  {
    std::vector<Attachment> colAttachments;
  };

  std::vector<Subpass> subpasses;
};

struct VkFramebuffer_T
{
  std::vector<VkImageView> attachments;
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

void InitFrameStats();
void BeginFrameStats();
void EndFrameStats();
void ShutdownFrameStats();

inline uint32_t hashSPV(const uint32_t *spv, size_t numWords)
{
  uint32_t hash = 5381;

  for(uint32_t i = 0; i < numWords; i++)
    hash = ((hash << 5) + hash) + spv[i]; /* hash * 33 + c */

  return hash;
}