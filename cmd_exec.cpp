#include "precompiled.h"
#include "commands.h"
#include "gpu.h"

template <typename T>
const T &pull(const byte **offs)
{
  const T *ret = (const T *)*offs;

  *offs += sizeof(T);

  return *ret;
}

void ClearTarget(VkImage target, const VkClearColorValue &col);

void DrawTriangles(VkImage target, int numVerts, const float *pos, const float *UV,
                   const float *MVP, const VkImage tex);

void VkCommandBuffer_T::execute() const
{
  const byte *cur = commandStream.data();
  const byte *end = cur + commandStream.size();

  GPUState state = {0};

  while(cur < end)
  {
    const Command &cmd = pull<Command>(&cur);

    switch(cmd)
    {
      case Command::PipelineBarrier:
      {
        const cmd::PipelineBarrier &data = pull<cmd::PipelineBarrier>(&cur);

        (void)data;
        break;
      }
      case Command::BeginRenderPass:
      {
        const cmd::BeginRenderPass &data = pull<cmd::BeginRenderPass>(&cur);

        VkRenderPass_T::Subpass &subpass = data.renderPass->subpasses[0];
        VkRenderPass_T::Attachment &att = subpass.colAttachments[0];

        uint32_t col = att.idx;

        state.target = data.framebuffer->attachments[col]->image;

        if(att.clear)
        {
          ClearTarget(state.target, data.clearval.color);
        }

        break;
      }
      case Command::EndRenderPass:
      {
        const cmd::EndRenderPass &data = pull<cmd::EndRenderPass>(&cur);

        state.target = VK_NULL_HANDLE;

        break;
      }
      case Command::BindPipeline:
      {
        const cmd::BindPipeline &data = pull<cmd::BindPipeline>(&cur);

        state.pipeline = data.pipeline;

        break;
      }
      case Command::BindDescriptorSets:
      {
        const cmd::BindDescriptorSets &data = pull<cmd::BindDescriptorSets>(&cur);

        state.set = data.set;

        break;
      }
      case Command::SetViewport:
      {
        const cmd::SetViewport &data = pull<cmd::SetViewport>(&cur);

        state.view = data.view;
        break;
      }
      case Command::SetScissors:
      {
        const cmd::SetScissors &data = pull<cmd::SetScissors>(&cur);

        (void)data;
        break;
      }
      case Command::Draw:
      {
        const cmd::Draw &data = pull<cmd::Draw>(&cur);

        // hack since we don't have shaders compiled to reflect...

        const VkBuffer ubo = state.set->binds[0].data.bufferInfo.buffer;
        VkDeviceSize offs = state.set->binds[0].data.bufferInfo.offset;

        const VkImage tex = state.set->binds[1].data.imageInfo.imageView->image;

        if(ubo && tex)
        {
          const float *mvp = (const float *)(ubo->bytes + offs);
          const float *pos = mvp + 4 * 4;
          const float *UV = pos + 12 * 3 * 4;

          DrawTriangles(state.target, data.vertexCount, pos, UV, mvp, tex);
        }
        break;
      }
    }
  }
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount,
                                             const VkSubmitInfo *pSubmits, VkFence fence)
{
  for(uint32_t i = 0; i < submitCount; i++)
  {
    for(uint32_t c = 0; c < pSubmits[i].commandBufferCount; c++)
    {
      pSubmits[i].pCommandBuffers[c]->execute();
    }
  }

  return VK_SUCCESS;
}
