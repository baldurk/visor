#include "wsi.h"

Swapchain::~Swapchain()
{
  for(Image *i : backbuffers)
    delete i;

  if(dc)
    ReleaseDC(wnd, dc);
}

SwapImage::SwapImage(HWND wnd, HDC windc) : Image(1, 1)
{
  RECT rect = {};
  GetClientRect(wnd, &rect);

  width = rect.right - rect.left;
  height = rect.bottom - rect.top;

  dc = CreateCompatibleDC(windc);

  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  info.bmiHeader.biSizeImage = 0;
  info.bmiHeader.biXPelsPerMeter = info.bmiHeader.biYPelsPerMeter = 96;
  info.bmiHeader.biClrUsed = 0;
  info.bmiHeader.biClrImportant = 0;

  bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
  assert(bmp && pixels);

  SelectObject(dc, bmp);
}

SwapImage::~SwapImage()
{
  if(dc)
    DeleteDC(dc);

  if(bmp)
    DeleteObject(bmp);

  pixels = NULL;
}

Swapchain *CreateSwapchain(HWND wnd, int numBuffers)
{
  Swapchain *ret = new Swapchain;

  ret->wnd = wnd;
  ret->dc = GetDC(wnd);
  ret->backbuffers.resize(numBuffers);
  for(int i = 0; i < numBuffers; i++)
    ret->backbuffers[i] = new SwapImage(ret->wnd, ret->dc);

  return ret;
}

void Destroy(Swapchain *swap)
{
  delete swap;
}

int Acquire(Swapchain *swap)
{
  swap->current = (swap->current + 1) % swap->backbuffers.size();
  return swap->current;
}

Image *GetImage(Swapchain *swap, int index)
{
  return swap->backbuffers[index];
}

void Present(Swapchain *swap, int index)
{
  SwapImage *backbuffer = swap->backbuffers[index];

  BitBlt(swap->dc, 0, 0, backbuffer->width, backbuffer->height, backbuffer->dc, 0, 0, SRCCOPY);
}
