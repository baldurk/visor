#include "precompiled.h"

MICROPROFILE_DECLARE(rasterizer_ShadeVerts);
MICROPROFILE_DECLARE(rasterizer_ProcessTriangles);
MICROPROFILE_DECLARE(rasterizer_ToWindow);
MICROPROFILE_DECLARE(rasterizer_MinMax);
MICROPROFILE_DECLARE(rasterizer_ClearTarget);
MICROPROFILE_DECLARE(rasterizer_DrawTriangles);
MICROPROFILE_DECLARE(vkQueueSubmit);
MICROPROFILE_DECLARE(vkQueuePresentKHR);

MICROPROFILE_DEFINE(rasterizer_ShadeVerts, "rasterizer", "ShadeVerts", MP_KHAKI);
MICROPROFILE_DEFINE(rasterizer_ProcessTriangles, "rasterizer", "ProcessTriangles", MP_CHOCOLATE);
MICROPROFILE_DEFINE(rasterizer_ToWindow, "rasterizer", "ToWindow", MP_MAROON);
MICROPROFILE_DEFINE(rasterizer_MinMax, "rasterizer", "MinMax", MP_FIREBRICK);
MICROPROFILE_DEFINE(rasterizer_ClearTarget, "rasterizer", "ClearTarget", MP_GAINSBORO);
MICROPROFILE_DEFINE(rasterizer_DrawTriangles, "rasterizer", "DrawTriangles", MP_THISTLE);
MICROPROFILE_DEFINE(vkQueueSubmit, "vulkan", "vkQueueSubmit", MP_RED);
MICROPROFILE_DEFINE(vkQueuePresentKHR, "vulkan", "vkQueuePresentKHR", MP_BLUE);

void InitFrameStats()
{
}

void BeginFrameStats()
{
  MICROPROFILE_COUNTER_SET("rasterizer/blocks/processed", 0);
  MICROPROFILE_COUNTER_SET("rasterizer/pixels/tested", 0);
  MICROPROFILE_COUNTER_SET("rasterizer/pixels/written", 0);
  MICROPROFILE_COUNTER_SET("rasterizer/depth/passed", 0);
  MICROPROFILE_COUNTER_SET("rasterizer/triangles/in", 0);
  MICROPROFILE_COUNTER_SET("rasterizer/triangles/out", 0);
  MICROPROFILE_COUNTER_SET("rasterizer/draws/in", 0);
  MICROPROFILE_COUNTER_SET("tcache/misses", 0);
  MICROPROFILE_COUNTER_SET("tcache/hits", 0);
}

void EndFrameStats()
{
  MicroProfileFlip(NULL);
}

void ShutdownFrameStats()
{
  MicroProfileShutdown();
}