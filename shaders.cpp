#include "precompiled.h"
#include "spirv_compile.h"

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice device,
                                                    const VkShaderModuleCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkShaderModule *pShaderModule)
{
  VkShaderModule ret = new VkShaderModule_T;

  uint32_t hash = hashSPV(pCreateInfo->pCode, pCreateInfo->codeSize / sizeof(uint32_t));

  ret->func = GetPremadeShader(hash);

  if(ret->func == NULL)
  {
    ret->handle = CompileFunction(pCreateInfo->pCode, pCreateInfo->codeSize / sizeof(uint32_t));

    if(ret->handle == NULL)
      return VK_ERROR_DEVICE_LOST;
  }

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
        if(mod->func)
          ret->vs = (VertexShader)mod->func;
        else
          ret->vs = (VertexShader)GetFuncPointer(mod->handle, pCreateInfos[i].pStages[s].pName);
      }
      else if(pCreateInfos[i].pStages[s].stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      {
        if(mod->func)
          ret->fs = (FragmentShader)mod->func;
        else
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
