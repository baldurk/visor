#pragma once

namespace cmd
{
struct PipelineBarrier
{
  static const Command CommandID = Command::PipelineBarrier;
};

struct BeginRenderPass
{
  static const Command CommandID = Command::BeginRenderPass;
  VkRenderPass renderPass;
  VkFramebuffer framebuffer;
  VkClearValue clearval[8];
};

struct EndRenderPass
{
  static const Command CommandID = Command::EndRenderPass;
};

struct BindPipeline
{
  static const Command CommandID = Command::BindPipeline;
  VkPipeline pipeline;
};

struct BindDescriptorSets
{
  static const Command CommandID = Command::BindDescriptorSets;
  VkDescriptorSet set;
};

struct BindVB
{
  static const Command CommandID = Command::BindVB;
  uint32_t slot;
  VkBuffer buffer;
  VkDeviceSize offset;
};

struct BindIB
{
  static const Command CommandID = Command::BindIB;
  VkBuffer buffer;
  VkDeviceSize offset;
  VkIndexType indexType;
};

struct SetViewport
{
  static const Command CommandID = Command::SetViewport;
  VkViewport view;
};

struct SetScissors
{
  static const Command CommandID = Command::SetScissors;
};

struct Draw
{
  static const Command CommandID = Command::Draw;
  uint32_t vertexCount, instanceCount, firstVertex, firstInstance;
};

struct DrawIndexed
{
  static const Command CommandID = Command::DrawIndexed;
  uint32_t indexCount, instanceCount, firstIndex, firstInstance;
  int32_t vertexOffset;
};

struct CopyBuf2Img
{
  static const Command CommandID = Command::CopyBuf2Img;
  VkBuffer srcBuffer;
  VkImage dstImage;
  VkBufferImageCopy region;
};

struct CopyBuf
{
  static const Command CommandID = Command::CopyBuf;
  VkBuffer srcBuffer;
  VkBuffer dstBuffer;
  VkBufferCopy region;
};
};
