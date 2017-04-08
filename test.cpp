#include <assert.h>
#include <math.h>
#include <windows.h>
#include "graphics.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if(msg == WM_CLOSE)
  {
    DestroyWindow(hwnd);
    return 0;
  }
  if(msg == WM_DESTROY)
  {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND create_window(HINSTANCE inst, LONG w, LONG h)
{
  const wchar_t *name = L"VisorTestProgram";

  WNDCLASSEX wc;
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = 0;
  wc.lpfnWndProc = WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = inst;
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszMenuName = NULL;
  wc.lpszClassName = name;
  wc.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

  if(!RegisterClassEx(&wc))
    assert(false);

  RECT wr = {0, 0, w, h};
  AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
  HWND wnd =
      CreateWindowEx(WS_EX_CLIENTEDGE, name, name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                     CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, inst, NULL);

  assert(wnd);

  return wnd;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  HWND wnd = create_window(hInstance, 400, 400);

  ShowWindow(wnd, SW_SHOW);
  UpdateWindow(wnd);

  Swapchain *swap = CreateSwapchain(wnd, 2);
  Image *images[2] = {
      GetImage(swap, 0), GetImage(swap, 1),
  };

  float time = 0.0f;

  MSG msg = {};
  while(true)
  {
    // Check to see if any messages are waiting in the queue
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      // Translate the message and dispatch it to WindowProc()
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // If the message is WM_QUIT, exit the while loop
    if(msg.message == WM_QUIT)
      break;

    int bbidx = Acquire(swap);

    // clang-format off
    float pos[] = {
      sinf(time + M_PI*0.0f/3.0f), cosf(time + M_PI*0.0f/3.0f), 0.0f, 1.0f,
      sinf(time + M_PI*2.0f/3.0f), cosf(time + M_PI*2.0f/3.0f), 0.0f, 1.0f,
      sinf(time + M_PI*4.0f/3.0f), cosf(time + M_PI*4.0f/3.0f), 0.0f, 1.0f,
    };
    // clang-format on

    DrawTriangle(images[bbidx], pos, sizeof(pos) / sizeof(float));

    Present(swap, bbidx);

    time += 0.01f;

    Sleep(40);
  }

  Destroy(swap);

  DestroyWindow(wnd);

  return 0;
}