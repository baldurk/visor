#include "rasterizer.h"

Image::Image(uint32_t w, uint32_t h) : width(w), height(h)
{
}

Image::~Image()
{
  delete[] pixels;
}

void DrawTriangle(Image *target, const float *pos, size_t posCount)
{
  byte *bits = target->pixels;
  const uint32_t w = target->width;
  const uint32_t h = target->height;

  for(uint32_t y = 0; y < h; y++)
  {
    for(uint32_t x = 0; x < w; x++)
    {
      uint32_t u = x, v = y;

      bits[y * w * 4 + x * 4 + 0] = byte(float(u * 256) / float(w));
      bits[y * w * 4 + x * 4 + 1] = byte(float(v * 256) / float(h));
      bits[y * w * 4 + x * 4 + 2] = 127;
    }
  }
}
