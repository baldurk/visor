#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <assert.h>
#include <stdint.h>
#include <windows.h>
#include <algorithm>
#include <vector>
#include "3rdparty/microprofile.h"

typedef unsigned char byte;

struct Image;
struct Swapchain;

Swapchain *CreateSwapchain(HWND wnd, int numBuffers);
Image *GetImage(Swapchain *swap, int index);
void Destroy(Swapchain *swap);

int Acquire(Swapchain *swap);
void DrawTriangle(Image *backbuffer, const float *pos, size_t posCount);
void Present(Swapchain *swap, int index);
