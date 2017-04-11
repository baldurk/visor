#pragma once

#include "graphics.h"

struct SwapImage
{
  SwapImage(HWND wnd, HDC windc);
  ~SwapImage();

  uint32_t width = 0, height = 0;
  byte *pixels = NULL;

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