#include "precompiled.h"
#include "commands.h"

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

        (void)data;
        break;
      }
      case Command::EndRenderPass:
      {
        const cmd::EndRenderPass &data = pull<cmd::EndRenderPass>(&cur);

        (void)data;
      }
      case Command::BindPipeline:
      {
        const cmd::BindPipeline &data = pull<cmd::BindPipeline>(&cur);

        (void)data;
        break;
      }
      case Command::BindDescriptorSets:
      {
        const cmd::BindDescriptorSets &data = pull<cmd::BindDescriptorSets>(&cur);

        (void)data;
        break;
      }
      case Command::SetViewport:
      {
        const cmd::SetViewport &data = pull<cmd::SetViewport>(&cur);

        (void)data;
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

        (void)data;
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
