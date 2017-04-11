#include "precompiled.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(
    VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pDescriptorSetLayout)
{
  VkDescriptorSetLayout ret = new VkDescriptorSetLayout_T;
  ret->bindingCount = pCreateInfo->bindingCount;
  *pDescriptorSetLayout = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice device,
                                                        VkDescriptorSetLayout descriptorSetLayout,
                                                        const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice device,
                                                      const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator,
                                                      VkPipelineLayout *pPipelineLayout)
{
  // TODO but for now return unique values
  static uint64_t nextPipelineLayout = 1;
  *pPipelineLayout = (VkPipelineLayout)(nextPipelineLayout++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout,
                                                   const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice device,
                                                      const VkDescriptorPoolCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator,
                                                      VkDescriptorPool *pDescriptorPool)
{
  // TODO but for now return unique values
  static uint64_t nextDescriptorPool = 1;
  *pDescriptorPool = (VkDescriptorPool)(nextDescriptorPool++);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool sampler,
                                                   const VkAllocationCallbacks *pAllocator)
{
  // nothing to do
}

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice device,
                                                        const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                        VkDescriptorSet *pDescriptorSets)
{
  for(uint32_t i = 0; i < pAllocateInfo->descriptorSetCount; i++)
  {
    pDescriptorSets[0] = new VkDescriptorSet_T;
    pDescriptorSets[0]->binds.resize(pAllocateInfo->pSetLayouts[i]->bindingCount);
  }
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool,
                                                    uint32_t descriptorSetCount,
                                                    const VkDescriptorSet *pDescriptorSets)
{
  for(uint32_t i = 0; i < descriptorSetCount; i++)
    delete pDescriptorSets[i];
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount,
                                                  const VkWriteDescriptorSet *pDescriptorWrites,
                                                  uint32_t descriptorCopyCount,
                                                  const VkCopyDescriptorSet *pDescriptorCopies)
{
  for(uint32_t i = 0; i < descriptorWriteCount; i++)
  {
    const VkWriteDescriptorSet &w = pDescriptorWrites[i];
    VkDescriptorSet_T::Bind &bind = w.dstSet->binds[w.dstBinding];
    bind.type = w.descriptorType;

    switch(w.descriptorType)
    {
      case VK_DESCRIPTOR_TYPE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: bind.data.imageInfo = w.pImageInfo[0]; break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        bind.data.texelBufferView = w.pTexelBufferView[0];
        break;
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        bind.data.bufferInfo = w.pBufferInfo[0];
        break;
    }
  }
}
