#include "precompiled.h"

VkDeviceSize CalcSubresourceByteOffset(VkImage img, uint32_t mip, uint32_t layer)
{
  VkDeviceSize offs = 0;

  const uint32_t w = img->extent.width;
  const uint32_t h = img->extent.height;
  const uint32_t bpp = img->bytesPerPixel;

  for(uint32_t m = 0; m < mip; m++)
  {
    const uint32_t mw = std::max(1U, w >> m);
    const uint32_t mh = std::max(1U, h >> m);
    offs += mw * mh * bpp;
  }

  if(layer > 0)
  {
    uint32_t mw = w;
    uint32_t mh = h;

    VkDeviceSize sliceSize = 0;

    do
    {
      sliceSize += mw * mh * bpp;
      mw = std::max(1U, mw >> 1);
      mh = std::max(1U, mh >> 1);
    } while(mw > 1 || mh > 1);

    offs += sliceSize * layer;
  }

  return offs;
}
