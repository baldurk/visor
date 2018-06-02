#include "precompiled.h"
#include "spirv_compile.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device,
                                                    const VkShaderModuleCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkShaderModule *pShaderModule)
{
  VkShaderModule ret = new VkShaderModule_T;

  ret->handle = CompileFunction(pCreateInfo->pCode, pCreateInfo->codeSize / sizeof(uint32_t));

  if(ret->handle == NULL)
    return VK_ERROR_DEVICE_LOST;

  *pShaderModule = ret;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule,
                                                 const VkAllocationCallbacks *pAllocator)
{
  DestroyFunction(shaderModule->handle);
  delete shaderModule;
}

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
                          const VkGraphicsPipelineCreateInfo *pCreateInfos,
                          const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
  for(uint32_t i = 0; i < createInfoCount; i++)
  {
    VkPipeline ret = new VkPipeline_T;

    uint32_t strides[16] = {0};

    for(uint32_t vb = 0; vb < pCreateInfos[i].pVertexInputState->vertexBindingDescriptionCount; vb++)
    {
      const VkVertexInputBindingDescription &bind =
          pCreateInfos[i].pVertexInputState->pVertexBindingDescriptions[vb];

      strides[bind.binding] = bind.stride;
    }

    for(uint32_t va = 0; va < pCreateInfos[i].pVertexInputState->vertexAttributeDescriptionCount; va++)
    {
      const VkVertexInputAttributeDescription &attr =
          pCreateInfos[i].pVertexInputState->pVertexAttributeDescriptions[va];

      ret->vattrs[attr.location].vb = attr.binding;
      ret->vattrs[attr.location].format = attr.format;
      ret->vattrs[attr.location].offset = attr.offset;
      ret->vattrs[attr.location].stride = strides[attr.binding];
    }

    ret->topology = pCreateInfos[i].pInputAssemblyState->topology;
    ret->frontFace = pCreateInfos[i].pRasterizationState->frontFace;
    ret->cullMode = pCreateInfos[i].pRasterizationState->cullMode;

    if(pCreateInfos[i].pDepthStencilState)
    {
      ret->depthCompareOp = pCreateInfos[i].pDepthStencilState->depthCompareOp;
      ret->depthWriteEnable = pCreateInfos[i].pDepthStencilState->depthWriteEnable == VK_TRUE;
    }

    if(pCreateInfos[i].pColorBlendState && pCreateInfos[i].pColorBlendState->attachmentCount > 0)
    {
      ret->blend = pCreateInfos[i].pColorBlendState->pAttachments[0];
    }
    else
    {
      memset(&ret->blend, 0, sizeof(ret->blend));
    }

    for(uint32_t s = 0; s < pCreateInfos[i].stageCount; s++)
    {
      VkShaderModule mod = pCreateInfos[i].pStages[s].module;
      if(pCreateInfos[i].pStages[s].stage == VK_SHADER_STAGE_VERTEX_BIT)
      {
        ret->vs = (VertexShader)GetFuncPointer(mod->handle, pCreateInfos[i].pStages[s].pName);
      }
      else if(pCreateInfos[i].pStages[s].stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        ret->fs = (FragmentShader)GetFuncPointer(mod->handle, pCreateInfos[i].pStages[s].pName);
      }
    }
    pPipelines[i] = ret;
  }
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
                         const VkComputePipelineCreateInfo *pCreateInfos,
                         const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
  for(uint32_t i = 0; i < createInfoCount; i++)
  {
    VkPipeline ret = new VkPipeline_T;
    pPipelines[i] = ret;
  }
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice device, VkPipeline pipeline,
                                             const VkAllocationCallbacks *pAllocator)
{
  delete pipeline;
}
