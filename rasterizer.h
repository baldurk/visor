#pragma once

#include "graphics.h"

struct Image
{
  Image(uint32_t w, uint32_t h);
  virtual ~Image();

  uint32_t width = 0, height = 0;
  byte *pixels = NULL;
};
