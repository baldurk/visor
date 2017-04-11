#include "precompiled.h"

void BeginFrameStats()
{
  MICROPROFILE_COUNTER_SET("rasterizer/pixels/tested", 0);
  MICROPROFILE_COUNTER_SET("rasterizer/pixels/written", 0);
  MICROPROFILE_COUNTER_SET("rasterizer/triangles/in", 0);
}

void EndFrameStats()
{
  MicroProfileFlip(NULL);
}
