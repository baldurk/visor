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

void DrawTriangles(const GPUState &state, int numVerts);

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
      case Command::BindIB:
      {
        const cmd::BindIB &data = pull<cmd::BindIB>(&cur);

        state.ib.buffer = data.buffer;
        state.ib.offset = data.offset;
        state.ib.indexType = data.indexType;

        break;
      };
      case Command::BindVB:
      {
        const cmd::BindVB &data = pull<cmd::BindVB>(&cur);

        state.vbs[data.slot].buffer = data.buffer;
        state.vbs[data.slot].offset = data.offset;

        break;
      };
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

        DrawTriangles(state, data.vertexCount);
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
