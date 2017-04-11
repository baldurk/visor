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
  VkClearValue clearval;
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
};
