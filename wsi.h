#pragma once

#include "rasterizer.h"

struct SwapImage : public Image
{
  SwapImage(HWND wnd, HDC windc);
  ~SwapImage();

  HDC dc = NULL;
  HBITMAP bmp = NULL;
};

struct Swapchain
{
  ~Swapchain();

  HWND wnd = NULL;
  HDC dc = NULL;
  std::vector<SwapImage *> backbuffers;
  int current = 0;
};