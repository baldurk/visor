#include "precompiled.h"

#define ICD_FUNC(name)      \
  if(!strcmp(pName, #name)) \
    return (PFN_vkVoidFunction)&name;

#define CHECK_INST_FUNCS()                          \
  ICD_FUNC(vkEnumerateInstanceExtensionProperties); \
  ICD_FUNC(vkEnumerateDeviceExtensionProperties);   \
  ICD_FUNC(vkGetDeviceProcAddr);                    \
  ICD_FUNC(vkCreateInstance);                       \
  ICD_FUNC(vkDestroyInstance);                      \
  ICD_FUNC(vkEnumeratePhysicalDevices);             \
  ICD_FUNC(vkCreateDevice);

#define CHECK_PHYS_FUNCS()                             \
  ICD_FUNC(vkGetPhysicalDeviceFeatures);               \
  ICD_FUNC(vkGetPhysicalDeviceProperties);             \
  ICD_FUNC(vkGetPhysicalDeviceQueueFamilyProperties);  \
  ICD_FUNC(vkGetPhysicalDeviceMemoryProperties);       \
  ICD_FUNC(vkGetPhysicalDeviceFormatProperties);       \
  ICD_FUNC(vkGetPhysicalDeviceSurfaceSupportKHR);      \
  ICD_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR);      \
  ICD_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR); \
  ICD_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR);

#define CHECK_DEV_FUNCS()                  \
  ICD_FUNC(vkDestroyDevice);               \
  ICD_FUNC(vkGetDeviceQueue);              \
  ICD_FUNC(vkCreateFence);                 \
  ICD_FUNC(vkDestroyFence);                \
  ICD_FUNC(vkCreateSemaphore);             \
  ICD_FUNC(vkDestroySemaphore);            \
  ICD_FUNC(vkCreateCommandPool);           \
  ICD_FUNC(vkDestroyCommandPool);          \
  ICD_FUNC(vkAllocateCommandBuffers);      \
  ICD_FUNC(vkFreeCommandBuffers);          \
  ICD_FUNC(vkBeginCommandBuffer);          \
  ICD_FUNC(vkEndCommandBuffer);            \
  ICD_FUNC(vkCreateSwapchainKHR);          \
  ICD_FUNC(vkDestroySwapchainKHR);         \
  ICD_FUNC(vkGetSwapchainImagesKHR);       \
  ICD_FUNC(vkAcquireNextImageKHR);         \
  ICD_FUNC(vkQueuePresentKHR);             \
  ICD_FUNC(vkCreateImageView);             \
  ICD_FUNC(vkDestroyImageView);            \
  ICD_FUNC(vkCreateImage);                 \
  ICD_FUNC(vkDestroyImage);                \
  ICD_FUNC(vkCreateBuffer);                \
  ICD_FUNC(vkDestroyBuffer);               \
  ICD_FUNC(vkGetBufferMemoryRequirements); \
  ICD_FUNC(vkGetImageMemoryRequirements);  \
  ICD_FUNC(vkBindBufferMemory);            \
  ICD_FUNC(vkBindImageMemory);             \
  ICD_FUNC(vkAllocateMemory);              \
  ICD_FUNC(vkFreeMemory);                  \
  ICD_FUNC(vkMapMemory);                   \
  ICD_FUNC(vkUnmapMemory);                 \
  ICD_FUNC(vkGetImageSubresourceLayout);   \
  ICD_FUNC(vkCreateSampler);               \
  ICD_FUNC(vkDestroySampler);              \
  ICD_FUNC(vkCreateDescriptorSetLayout);   \
  ICD_FUNC(vkDestroyDescriptorSetLayout);  \
  ICD_FUNC(vkCreatePipelineLayout);        \
  ICD_FUNC(vkDestroyPipelineLayout);       \
  ICD_FUNC(vkCreateFramebuffer);           \
  ICD_FUNC(vkDestroyFramebuffer);          \
  ICD_FUNC(vkCreateRenderPass);            \
  ICD_FUNC(vkDestroyRenderPass);           \
  ICD_FUNC(vkCreateShaderModule);          \
  ICD_FUNC(vkDestroyShaderModule);         \
  ICD_FUNC(vkCreatePipelineCache);         \
  ICD_FUNC(vkDestroyPipelineCache);        \
  ICD_FUNC(vkCreateGraphicsPipelines);     \
  ICD_FUNC(vkCreateComputePipelines);      \
  ICD_FUNC(vkDestroyPipeline);             \
  ICD_FUNC(vkCreateDescriptorPool);        \
  ICD_FUNC(vkDestroyDescriptorPool);       \
  ICD_FUNC(vkAllocateDescriptorSets);      \
  ICD_FUNC(vkFreeDescriptorSets);          \
  ICD_FUNC(vkUpdateDescriptorSets);        \
  ICD_FUNC(vkCmdPipelineBarrier);          \
  ICD_FUNC(vkCmdBeginRenderPass);          \
  ICD_FUNC(vkCmdEndRenderPass);            \
  ICD_FUNC(vkCmdBindPipeline);             \
  ICD_FUNC(vkCmdBindDescriptorSets);       \
  ICD_FUNC(vkCmdSetViewport);              \
  ICD_FUNC(vkCmdSetScissor);               \
  ICD_FUNC(vkCmdDraw);                     \
  ICD_FUNC(vkQueueSubmit);                 \
  ICD_FUNC(vkWaitForFences);               \
  ICD_FUNC(vkResetFences);                 \
  ICD_FUNC(vkDeviceWaitIdle);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName)
{
  CHECK_DEV_FUNCS();

  // we want to return non-NULL for all entry points otherwise our ICD will be discarded. We've
  // implemented all the functions needed for vkcube so if any other function gets called it will
  // execute a string and we can see what function was missing.
  return (PFN_vkVoidFunction)strdup(pName);
}

extern "C" {

#if defined(_WIN32)
#undef VKAPI_ATTR
#define VKAPI_ATTR __declspec(dllexport)
#endif

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance,
                                                                   const char *pName)
{
  CHECK_INST_FUNCS();
  CHECK_PHYS_FUNCS();
  CHECK_DEV_FUNCS();

  if(!strcmp(pName, "vkCreateDebugReportCallbackEXT"))
    return NULL;
  if(!strcmp(pName, "vkCreateWin32SurfaceKHR"))
    return NULL;

  // we want to return non-NULL for all entry points otherwise our ICD will be discarded. We've
  // implemented all the functions needed for vkcube so if any other function gets called it will
  // execute a string and we can see what function was missing.
  return (PFN_vkVoidFunction)strdup(pName);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetPhysicalDeviceProcAddr(VkInstance instance,
                                                                         const char *pName)
{
  CHECK_PHYS_FUNCS();

  // we want to return non-NULL for all entry points otherwise our ICD will be discarded. We've
  // implemented all the functions needed for vkcube so if any other function gets called it will
  // execute a string and we can see what function was missing.
  return (PFN_vkVoidFunction)strdup(pName);
}

VKAPI_ATTR VkResult VKAPI_CALL vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pSupportedVersion)
{
  if(!pSupportedVersion)
    return VK_ERROR_INITIALIZATION_FAILED;

  *pSupportedVersion = std::min<uint32_t>(CURRENT_LOADER_ICD_INTERFACE_VERSION, *pSupportedVersion);

  return VK_SUCCESS;
}

};    // extern "C"