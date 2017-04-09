#include "rasterizer.h"

Image *MakeImage(uint32_t width, uint32_t height, const byte *rgbaPixels)
{
  Image *ret = new Image(width, height);
  ret->pixels = new byte[width * height * 4];
  memcpy(ret->pixels, rgbaPixels, width * height * 4);
  return ret;
}

void Destroy(Image *image)
{
  delete image;
}
