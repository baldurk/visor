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
        VkRenderPass_T::Attachment &col0 = subpass.colAttachments[0];

        state.col[0] = data.framebuffer->attachments[col0.idx]->image;

        int clearIdx = 0;

        if(col0.clear)
        {
          ClearTarget(state.col[0], data.clearval[clearIdx++].color);
        }

        VkRenderPass_T::Attachment &depth = subpass.depthAttachment;

        if(depth.idx >= 0)
        {
          state.depth = data.framebuffer->attachments[depth.idx]->image;

          if(depth.clear)
          {
            ClearTarget(state.depth, data.clearval[clearIdx++].depthStencil);
          }
        }

        break;
      }
      case Command::EndRenderPass:
      {
        const cmd::EndRenderPass &data = pull<cmd::EndRenderPass>(&cur);

        state.col[0] = VK_NULL_HANDLE;

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

        DrawTriangles(state, data.vertexCount, data.firstVertex, false);
        break;
      }
      case Command::DrawIndexed:
      {
        const cmd::DrawIndexed &data = pull<cmd::DrawIndexed>(&cur);

        DrawTriangles(state, data.indexCount, data.firstIndex, true);
        break;
      }
      case Command::CopyBuf2Img:
      {
        const cmd::CopyBuf2Img &data = pull<cmd::CopyBuf2Img>(&cur);

        // only support tight packed copies right now
        assert(data.region.bufferRowLength == 0 && data.region.bufferImageHeight == 0);

        // only support non-offseted copies
        assert(data.region.imageOffset.x == 0 && data.region.imageOffset.y == 0 &&
               data.region.imageOffset.z == 0);

        uint32_t mip = data.region.imageSubresource.mipLevel;

        const uint32_t w = std::max(1U, data.dstImage->extent.width >> mip);
        const uint32_t h = std::max(1U, data.dstImage->extent.height >> mip);
        const uint32_t bpp = data.dstImage->bytesPerPixel;

        // only support copying the whole mip
        assert(w == data.region.imageExtent.width && h == data.region.imageExtent.height);

        // only support copying one layer at a time
        assert(data.region.imageSubresource.layerCount == 1);

        VkDeviceSize offs = CalcSubresourceByteOffset(data.dstImage, mip,
                                                      data.region.imageSubresource.baseArrayLayer);

        memcpy(data.dstImage->pixels + offs, data.srcBuffer->bytes + data.region.bufferOffset,
               w * h * bpp);

        break;
      }
      case Command::CopyBuf:
      {
        const cmd::CopyBuf &data = pull<cmd::CopyBuf>(&cur);

        memcpy(data.dstBuffer->bytes + data.region.dstOffset,
               data.srcBuffer->bytes + data.region.srcOffset, data.region.size);

        break;
      }
    }
  }
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount,
                                             const VkSubmitInfo *pSubmits, VkFence fence)
{
  MICROPROFILE_SCOPE(vkQueueSubmit);

  for(uint32_t i = 0; i < submitCount; i++)
  {
    for(uint32_t c = 0; c < pSubmits[i].commandBufferCount; c++)
    {
      pSubmits[i].pCommandBuffers[c]->execute();
    }
  }

  return VK_SUCCESS;
}
