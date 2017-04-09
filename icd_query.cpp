#include "icd_common.h"

static VkResult FillPropertyCountAndList(const VkExtensionProperties *src, uint32_t numExts,
                                         uint32_t *dstCount, VkExtensionProperties *dstProps)
{
  if(dstCount && !dstProps)
  {
    // just returning the number of extensions
    *dstCount = numExts;
    return VK_SUCCESS;
  }
  else if(dstCount && dstProps)
  {
    uint32_t dstSpace = *dstCount;

    // return the number of extensions.
    *dstCount = std::min(numExts, dstSpace);

    // copy as much as there's space for, up to how many there are
    memcpy(dstProps, src, sizeof(VkExtensionProperties) * std::min(numExts, dstSpace));

    // if there was enough space, return success, else incomplete
    if(dstSpace >= numExts)
      return VK_SUCCESS;
    else
      return VK_INCOMPLETE;
  }

  // both parameters were NULL, return incomplete
  return VK_INCOMPLETE;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
    const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties)
{
  assert(pLayerName == NULL);

  static const VkExtensionProperties exts[] = {
    {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_SURFACE_SPEC_VERSION},
#if defined(_WIN32)
    {VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_SPEC_VERSION},
#endif
  };

  return FillPropertyCountAndList(exts, (uint32_t)sizeof(exts) / sizeof(exts[0]), pPropertyCount,
                                  pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                                                    const char *pLayerName,
                                                                    uint32_t *pPropertyCount,
                                                                    VkExtensionProperties *pProperties)
{
  assert(pLayerName == NULL);

  static const VkExtensionProperties exts[] = {
      {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SWAPCHAIN_SPEC_VERSION},
  };

  return FillPropertyCountAndList(exts, (uint32_t)sizeof(exts) / sizeof(exts[0]), pPropertyCount,
                                  pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance instance,
                                                          uint32_t *pPhysicalDeviceCount,
                                                          VkPhysicalDevice *pPhysicalDevices)
{
  // one physical device. In theory we could expose a dozen though and it wouldn't matter
  if(pPhysicalDeviceCount && !pPhysicalDevices)
  {
    *pPhysicalDeviceCount = 1;
    return VK_SUCCESS;
  }

  *pPhysicalDevices = (VkPhysicalDevice) new VK_LOADER_DATA;
  set_loader_magic_value(*pPhysicalDevices);
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice,
                                                       VkPhysicalDeviceFeatures *pFeatures)
{
  memset(pFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
  pFeatures->fullDrawIndexUint32 = VK_TRUE;
  pFeatures->fillModeNonSolid = VK_TRUE;
  // CTSTODO - we would need to support one of the compressed texture formats here
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                                                         VkPhysicalDeviceProperties *pProperties)
{
  memset(pProperties, 0, sizeof(VkPhysicalDeviceProperties));

  pProperties->apiVersion = VK_MAKE_VERSION(1, 0, 47);
  pProperties->driverVersion = VK_MAKE_VERSION(0, 1, 0);
  pProperties->vendorID = 0x10003;
  pProperties->deviceID = 0x01234;
  pProperties->deviceType = VK_PHYSICAL_DEVICE_TYPE_CPU;
  char devName[] = "Visor Software Renderer (https://github.com/baldurk/visor)";
  memcpy(pProperties->deviceName, devName, sizeof(devName));
  for(int i = 0; i < VK_UUID_SIZE; i++)
    pProperties->pipelineCacheUUID[i] = uint8_t(i);

  // minimum set of limits. We can increase this in future when we know what we can support above
  // this minimum.
  VkSampleCountFlags minSampleCounts = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;
  pProperties->limits = {
      /* uint32_t maxImageDimension1D = */ 4096,
      /* uint32_t maxImageDimension2D = */ 4096,
      /* uint32_t maxImageDimension3D = */ 256,
      /* uint32_t maxImageDimensionCube = */ 4096,
      /* uint32_t maxImageArrayLayers = */ 256,
      /* uint32_t maxTexelBufferElements = */ 65536,
      /* uint32_t maxUniformBufferRange = */ 16384,
      /* uint32_t maxStorageBufferRange = */ 1U << 27,
      /* uint32_t maxPushConstantsSize = */ 128,
      /* uint32_t maxMemoryAllocationCount = */ 4096,
      /* uint32_t maxSamplerAllocationCount = */ 4000,
      /* VkDeviceSize bufferImageGranularity = */ 131072,
      /* VkDeviceSize sparseAddressSpaceSize = */ 0,
      /* uint32_t maxBoundDescriptorSets = */ 4,
      /* uint32_t maxPerStageDescriptorSamplers = */ 16,
      /* uint32_t maxPerStageDescriptorUniformBuffers = */ 12,
      /* uint32_t maxPerStageDescriptorStorageBuffers = */ 4,
      /* uint32_t maxPerStageDescriptorSampledImages = */ 16,
      /* uint32_t maxPerStageDescriptorStorageImages = */ 4,
      /* uint32_t maxPerStageDescriptorInputAttachments = */ 4,
      /* uint32_t maxPerStageResources = */ 128,
      /* uint32_t maxDescriptorSetSamplers = */ 96,
      /* uint32_t maxDescriptorSetUniformBuffers = */ 72,
      /* uint32_t maxDescriptorSetUniformBuffersDynamic = */ 8,
      /* uint32_t maxDescriptorSetStorageBuffers = */ 24,
      /* uint32_t maxDescriptorSetStorageBuffersDynamic = */ 4,
      /* uint32_t maxDescriptorSetSampledImages = */ 96,
      /* uint32_t maxDescriptorSetStorageImages = */ 24,
      /* uint32_t maxDescriptorSetInputAttachments = */ 4,
      /* uint32_t maxVertexInputAttributes = */ 16,
      /* uint32_t maxVertexInputBindings = */ 16,
      /* uint32_t maxVertexInputAttributeOffset = */ 2047,
      /* uint32_t maxVertexInputBindingStride = */ 2048,
      /* uint32_t maxVertexOutputComponents = */ 64,
      /* uint32_t maxTessellationGenerationLevel = */ 0,
      /* uint32_t maxTessellationPatchSize = */ 0,
      /* uint32_t maxTessellationControlPerVertexInputComponents = */ 0,
      /* uint32_t maxTessellationControlPerVertexOutputComponents = */ 0,
      /* uint32_t maxTessellationControlPerPatchOutputComponents = */ 0,
      /* uint32_t maxTessellationControlTotalOutputComponents = */ 0,
      /* uint32_t maxTessellationEvaluationInputComponents = */ 0,
      /* uint32_t maxTessellationEvaluationOutputComponents = */ 0,
      /* uint32_t maxGeometryShaderInvocations = */ 0,
      /* uint32_t maxGeometryInputComponents = */ 0,
      /* uint32_t maxGeometryOutputComponents = */ 0,
      /* uint32_t maxGeometryOutputVertices = */ 0,
      /* uint32_t maxGeometryTotalOutputComponents = */ 0,
      /* uint32_t maxFragmentInputComponents = */ 64,
      /* uint32_t maxFragmentOutputAttachments = */ 4,
      /* uint32_t maxFragmentDualSrcAttachments = */ 0,
      /* uint32_t maxFragmentCombinedOutputResources = */ 4,
      /* uint32_t maxComputeSharedMemorySize = */ 16384,
      /* uint32_t maxComputeWorkGroupCount[3] = */ {65536, 65536, 65536},
      /* uint32_t maxComputeWorkGroupInvocations = */ 128,
      /* uint32_t maxComputeWorkGroupSize[3] = */ {128, 128, 64},
      /* uint32_t subPixelPrecisionBits = */ 4,
      /* uint32_t subTexelPrecisionBits = */ 4,
      /* uint32_t mipmapPrecisionBits = */ 4,
      /* uint32_t maxDrawIndexedIndexValue = */ UINT32_MAX - 1,
      /* uint32_t maxDrawIndirectCount = */ 1,
      /* float maxSamplerLodBias = */ 2,
      /* float maxSamplerAnisotropy = */ 1,
      /* uint32_t maxViewports = */ 1,
      /* uint32_t maxViewportDimensions[2] = */ {4096, 4096},
      /* float viewportBoundsRange[2] = */ {-8192.0f, 8191.0f},
      /* uint32_t viewportSubPixelBits = */ 0,
      /* size_t minMemoryMapAlignment = */ 64,
      /* VkDeviceSize minTexelBufferOffsetAlignment = */ 256,
      /* VkDeviceSize minUniformBufferOffsetAlignment = */ 256,
      /* VkDeviceSize minStorageBufferOffsetAlignment = */ 256,
      /* int32_t minTexelOffset = */ -8,
      /* uint32_t maxTexelOffset = */ 7,
      /* int32_t minTexelGatherOffset = */ 0,
      /* uint32_t maxTexelGatherOffset = */ 0,
      /* float minInterpolationOffset = */ 0.0f,
      /* float maxInterpolationOffset = */ 0.0f,
      /* uint32_t subPixelInterpolationOffsetBits = */ 0,
      /* uint32_t maxFramebufferWidth = */ 4096,
      /* uint32_t maxFramebufferHeight = */ 4096,
      /* uint32_t maxFramebufferLayers = */ 256,
      /* VkSampleCountFlags framebufferColorSampleCounts = */ minSampleCounts,
      /* VkSampleCountFlags framebufferDepthSampleCounts = */ minSampleCounts,
      /* VkSampleCountFlags framebufferStencilSampleCounts = */ minSampleCounts,
      /* VkSampleCountFlags framebufferNoAttachmentsSampleCounts = */ minSampleCounts,
      /* uint32_t maxColorAttachments = */ 4,
      /* VkSampleCountFlags sampledImageColorSampleCounts = */ minSampleCounts,
      /* VkSampleCountFlags sampledImageIntegerSampleCounts = */ VK_SAMPLE_COUNT_1_BIT,
      /* VkSampleCountFlags sampledImageDepthSampleCounts = */ minSampleCounts,
      /* VkSampleCountFlags sampledImageStencilSampleCounts = */ minSampleCounts,
      /* VkSampleCountFlags storageImageSampleCounts = */ VK_SAMPLE_COUNT_1_BIT,
      /* uint32_t maxSampleMaskWords = */ 1,
      /* VkBool32 timestampComputeAndGraphics = */ VK_TRUE,
      /* float timestampPeriod = */ 1,
      /* uint32_t maxClipDistances = */ 0,
      /* uint32_t maxCullDistances = */ 0,
      /* uint32_t maxCombinedClipAndCullDistances = */ 0,
      /* uint32_t discreteQueuePriorities = */ 2,
      /* float pointSizeRange[2] = */ {1.0f, 1.0f},
      /* float lineWidthRange[2] = */ {1.0f, 1.0f},
      /* float pointSizeGranularity = */ 0.0f,
      /* float lineWidthGranularity = */ 0.0f,
      /* VkBool32 strictLines = */ VK_FALSE,
      /* VkBool32 standardSampleLocations = */ VK_TRUE,
      /* VkDeviceSize optimalBufferCopyOffsetAlignment = */ 1,
      /* VkDeviceSize optimalBufferCopyRowPitchAlignment = */ 1,
      /* VkDeviceSize nonCoherentAtomSize = */ 1,
  };

  // sparse properties are all false
  pProperties->sparseProperties = {VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE, VK_FALSE};
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount,
    VkQueueFamilyProperties *pQueueFamilyProperties)
{
  // one do-it-all queue family
  if(pQueueFamilyPropertyCount && !pQueueFamilyProperties)
  {
    *pQueueFamilyPropertyCount = 1;
    return;
  }

  memset(pQueueFamilyProperties, 0, sizeof(VkQueueFamilyProperties));

  // we can do byte-granularity copies
  pQueueFamilyProperties->minImageTransferGranularity.width = 1;
  pQueueFamilyProperties->minImageTransferGranularity.height = 1;
  pQueueFamilyProperties->minImageTransferGranularity.depth = 1;

  // one do-it-all queue
  pQueueFamilyProperties->queueCount = 1;
  pQueueFamilyProperties->queueFlags =
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

  pQueueFamilyProperties->timestampValidBits = 64;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties *pMemoryProperties)
{
  memset(pMemoryProperties, 0, sizeof(VkPhysicalDeviceMemoryProperties));

  // we could get away with one heap, but let's have a separate 'GPU' and 'CPU' heap
  // of 1GB each
  pMemoryProperties->memoryHeapCount = 2;
  pMemoryProperties->memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
  pMemoryProperties->memoryHeaps[0].size = 1ULL << 30;
  pMemoryProperties->memoryHeaps[1].flags = 0;
  pMemoryProperties->memoryHeaps[1].size = 1ULL << 30;

  // keeping things simple, one type per heap
  pMemoryProperties->memoryTypeCount = 2;
  pMemoryProperties->memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  pMemoryProperties->memoryTypes[0].heapIndex = 0;
  pMemoryProperties->memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  pMemoryProperties->memoryTypes[1].heapIndex = 1;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice,
                                                               VkFormat format,
                                                               VkFormatProperties *pFormatProperties)
{
  // placeholder, just allow enough stuff without looking at the format
  pFormatProperties->bufferFeatures =
      VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT | VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT |
      VK_FORMAT_FEATURE_TRANSFER_SRC_BIT_KHR | VK_FORMAT_FEATURE_TRANSFER_DST_BIT_KHR;
  pFormatProperties->linearTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
  pFormatProperties->optimalTilingFeatures = pFormatProperties->linearTilingFeatures;
}
