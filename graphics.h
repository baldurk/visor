#pragma once

#include <stdint.h>
#include <windows.h>

struct Image;
struct Swapchain;

Swapchain *CreateSwapchain(HWND wnd, int numBuffers);
Image *GetImage(Swapchain *swap, int index);
void Destroy(Swapchain *swap);

int Acquire(Swapchain *swap);
void DrawTriangle(Image *backbuffer, const float *pos, size_t posCount);
void Present(Swapchain *swap, int index);
