#pragma once

void InitFrameStats();
void BeginFrameStats();
void EndFrameStats();
void ShutdownFrameStats();

MICROPROFILE_DECLARE(rasterizer_ShadeVerts);
MICROPROFILE_DECLARE(rasterizer_ProcessTriangles);
MICROPROFILE_DECLARE(rasterizer_ToWindow);
MICROPROFILE_DECLARE(rasterizer_MinMax);
MICROPROFILE_DECLARE(rasterizer_ClearTarget);
MICROPROFILE_DECLARE(rasterizer_DrawTriangles);
MICROPROFILE_DECLARE(vkQueueSubmit);
MICROPROFILE_DECLARE(vkQueuePresentKHR);