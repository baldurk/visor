#include "rasterizer.h"

struct int4
{
  int4() {}
  int4(int X, int Y, int Z, int W) : x(X), y(Y), z(Z), w(W) {}
  int x, y, z, w;
};

struct float4
{
  float4() {}
  float4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
  float x, y, z, w;
};

Image::Image(uint32_t w, uint32_t h) : width(w), height(h)
{
}

Image::~Image()
{
  delete[] pixels;
}

std::vector<int4> ToWindow(uint32_t w, uint32_t h, const float *pos, size_t posCount)
{
  MICROPROFILE_SCOPEI("rasterizer", "ToWindow", MP_GREEN);

  std::vector<int4> ret;

  for(size_t v = 0; v < posCount; v += 4)
  {
    int4 win = {};

    win.x = int((pos[0] + 1.0f) * 0.5f * w);
    win.y = int((pos[1] + 1.0f) * 0.5f * h);

    ret.push_back(win);
    pos += 4;
  }

  return ret;
}

void MinMax(const std::vector<int4> &coords, int4 &minwin, int4 &maxwin)
{
  MICROPROFILE_SCOPEI("rasterizer", "MinMax", MP_PURPLE);

  minwin = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
  maxwin = {INT_MIN, INT_MIN, INT_MIN, INT_MIN};

  for(int i = 0; i < coords.size(); i++)
  {
    minwin.x = std::min(minwin.x, coords[i].x);
    minwin.y = std::min(minwin.y, coords[i].y);
    minwin.z = std::min(minwin.z, coords[i].z);
    minwin.w = std::min(minwin.w, coords[i].w);

    maxwin.x = std::max(maxwin.x, coords[i].x);
    maxwin.y = std::max(maxwin.y, coords[i].y);
    maxwin.z = std::max(maxwin.z, coords[i].z);
    maxwin.w = std::max(maxwin.w, coords[i].w);
  }
}

int4 cross(int4 a, int4 b)
{
  return int4((a.y * b.z) - (b.y * a.z), (a.z * b.x) - (b.z * a.x), (a.x * b.y) - (b.x * a.y), 1);
}

int4 barycentric(int4 *verts, int4 pixel)
{
  int4 u = cross(int4(verts[1].x - verts[0].x, verts[2].x - verts[0].x, verts[0].x - pixel.x, 1),
                 int4(verts[1].y - verts[0].y, verts[2].y - verts[0].y, verts[0].y - pixel.y, 1));

  if(u.z == 0)
    return int4(-1, -1, -1, -1);

  return int4(u.y, u.x, u.z - (u.x + u.y), u.z);
}

void DrawTriangle(Image *target, const float *pos, size_t posCount)
{
  byte *bits = target->pixels;
  const uint32_t w = target->width;
  const uint32_t h = target->height;

  std::vector<int4> winCoords = ToWindow(w, h, pos, posCount);

  int4 minwin, maxwin;
  MinMax(winCoords, minwin, maxwin);

  {
    MICROPROFILE_SCOPEI("rasterizer", "clear RTV", MP_RED);
    memset(bits, 0x80, w * h * 4);
  }

  assert(minwin.x >= 0 && maxwin.x <= (int)w);
  assert(minwin.y >= 0 && maxwin.y <= (int)h);

  {
    MICROPROFILE_SCOPEI("rasterizer", "MainLoop", MP_BLUE);

    int written = 0, tested = 0;
    MICROPROFILE_COUNTER_SET("rasterizer/pixels/tested", tested);
    MICROPROFILE_COUNTER_SET("rasterizer/pixels/written", written);

    for(int y = minwin.y; y < maxwin.y; y++)
    {
      for(int x = minwin.x; x < maxwin.x; x++)
      {
        int4 b = barycentric(winCoords.data(), int4(x, y, 0, 0));

        if(b.x > 0.0f && b.y > 0.0f && b.z > 0.0f)
        {
          // normalise the barycentrics
          float4 n = float4(float(b.x), float(b.y), float(b.z), float(b.w));
          n.x /= n.w;
          n.y /= n.w;
          n.z /= n.w;

          bits[y * w * 4 + x * 4 + 0] = byte(n.x * 256);
          bits[y * w * 4 + x * 4 + 1] = byte(n.y * 256);
          bits[y * w * 4 + x * 4 + 2] = byte(n.z * 256);

          written++;
        }
        else
        {
          bits[y * w * 4 + x * 4 + 0] = 0x40;
          bits[y * w * 4 + x * 4 + 1] = 0x40;
          bits[y * w * 4 + x * 4 + 2] = 0x40;
        }

        tested++;
      }
    }

    MICROPROFILE_COUNTER_SET("rasterizer/pixels/tested", tested);
    MICROPROFILE_COUNTER_SET("rasterizer/pixels/written", written);
  }
}
