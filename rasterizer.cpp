#include "precompiled.h"

struct int4
{
  int4() {}
  int4(int X, int Y, int Z, int W) : x(X), y(Y), z(Z), w(W) {}
  union
  {
    struct
    {
      int x, y, z, w;
    };
    int v[4];
  };
};

struct float4
{
  float4() {}
  float4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
  union
  {
    struct
    {
      float x, y, z, w;
    };
    float v[4];
  };
};

static std::vector<float4> VertexShader(const float *pos, int numVerts, const float *MVP)
{
  MICROPROFILE_SCOPEI("rasterizer", "VertexShader", MP_KHAKI);

  std::vector<float4> ret;

  for(int v = 0; v < numVerts; v++)
  {
    float4 view(0.0f, 0.0f, 0.0f, 0.0f);

    for(int row = 0; row < 4; row++)
    {
      for(int col = 0; col < 4; col++)
        view.v[row] += MVP[col * 4 + row] * pos[col];
    }

    ret.push_back(view);
    pos += 4;
  }

  return ret;
}

static std::vector<int4> ToWindow(uint32_t w, uint32_t h, const std::vector<float4> &pos)
{
  MICROPROFILE_SCOPEI("rasterizer", "ToWindow", MP_GREEN);

  std::vector<int4> ret;

  for(const float4 &v : pos)
  {
    int4 win(0, 0, 0, 0);

    win.x = int((v.x / v.w + 1.0f) * 0.5f * w);
    win.y = int((v.y * -1.0f / v.w + 1.0f) * 0.5f * h);

    ret.push_back(win);
  }

  return ret;
}

static void MinMax(const int4 *coords, int4 &minwin, int4 &maxwin)
{
  MICROPROFILE_SCOPEI("rasterizer", "MinMax", MP_PURPLE);

  minwin = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
  maxwin = {INT_MIN, INT_MIN, INT_MIN, INT_MIN};

  for(int i = 0; i < 3; i++)
  {
    for(int c = 0; c < 4; c++)
    {
      minwin.v[c] = std::min(minwin.v[c], coords[i].v[c]);
      maxwin.v[c] = std::max(maxwin.v[c], coords[i].v[c]);
    }
  }
}

static int4 cross(int4 a, int4 b)
{
  return int4((a.y * b.z) - (b.y * a.z), (a.z * b.x) - (b.z * a.x), (a.x * b.y) - (b.x * a.y), 1);
}

static float dot(const float4 &a, const float4 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

static int4 barycentric(const int4 *verts, const int4 &pixel)
{
  int4 u = cross(int4(verts[1].x - verts[0].x, verts[2].x - verts[0].x, verts[0].x - pixel.x, 1),
                 int4(verts[1].y - verts[0].y, verts[2].y - verts[0].y, verts[0].y - pixel.y, 1));

  if(u.z == 0)
    return int4(-1, -1, -1, -1);

  return int4(u.z - (u.x + u.y), u.x, u.y, u.z);
}

static float4 PixelShader(float4 bary, float pixdepth, const float4 *homog, const float *UV,
                          const VkImage tex)
{
  MICROPROFILE_SCOPEI("rasterizer", "PixelShader", MP_WHITE);

  float u = dot(bary, float4(UV[0], UV[4], UV[8], 0.0f));
  float v = dot(bary, float4(UV[1], UV[5], UV[9], 0.0f));

  if(tex == NULL)
    return float4(u, v, 0.0f, 1.0f);

  if(u >= 1.0f)
    u -= 1.0f;
  if(v >= 1.0f)
    v -= 1.0f;

  u *= tex->extent.width;
  v *= tex->extent.height;

  int iu0 = int(u);
  int iv0 = int(v);
  int iu1 = iu0 + 1;
  int iv1 = iv0 + 1;

  if(iu1 >= (int)tex->extent.width)
    iu1 -= tex->extent.width;
  if(iv1 >= (int)tex->extent.height)
    iv1 -= tex->extent.height;

  float fu = u - float(iu0);
  float fv = v - float(iv0);

  byte *TL = &tex->pixels[(iv0 * tex->extent.width + iu0) * 4];
  byte *TR = &tex->pixels[(iv0 * tex->extent.width + iu1) * 4];
  byte *BL = &tex->pixels[(iv1 * tex->extent.width + iu0) * 4];
  byte *BR = &tex->pixels[(iv1 * tex->extent.width + iu1) * 4];

  float4 top;
  top.x = float(TL[0]) * (1.0f - fu) + float(TR[0]) * fu;
  top.y = float(TL[1]) * (1.0f - fu) + float(TR[1]) * fu;
  top.z = float(TL[1]) * (1.0f - fu) + float(TR[1]) * fu;

  float4 bottom;
  bottom.x = float(BL[0]) * (1.0f - fu) + float(BR[0]) * fu;
  bottom.y = float(BL[1]) * (1.0f - fu) + float(BR[1]) * fu;
  bottom.z = float(BL[1]) * (1.0f - fu) + float(BR[1]) * fu;

  float4 ret;
  ret.x = top.x * (1.0f - fv) + bottom.x * fv;
  ret.y = top.y * (1.0f - fv) + bottom.y * fv;
  ret.z = top.z * (1.0f - fv) + bottom.z * fv;

  return ret;
}

void ClearTarget(VkImage target, const VkClearColorValue &col)
{
  MICROPROFILE_SCOPEI("rasterizer", "clear RTV", MP_RED);

  byte *bits = target->pixels;
  const uint32_t w = target->extent.width;
  const uint32_t h = target->extent.height;

  byte eval[4];
  eval[2] = byte(col.float32[0] * 255.0f);
  eval[1] = byte(col.float32[1] * 255.0f);
  eval[0] = byte(col.float32[2] * 255.0f);
  eval[3] = byte(col.float32[3] * 255.0f);

  for(uint32_t x = 0; x < w; x++)
  {
    for(uint32_t y = 0; y < h; y++)
    {
      memcpy(&bits[y * w * 4 + x * 4], eval, sizeof(eval));
    }
  }
}

void DrawTriangles(VkImage target, int numVerts, const float *pos, const float *UV,
                   const float *MVP, const VkImage tex)
{
  byte *bits = target->pixels;
  const uint32_t w = target->extent.width;
  const uint32_t h = target->extent.height;

  std::vector<float4> homogCoords = VertexShader(pos, numVerts, MVP);

  std::vector<int4> winCoords = ToWindow(w, h, homogCoords);

  int written = 0, tested = 0, tris_in = 0;

  const int4 *tri = winCoords.data();
  const float4 *homog = homogCoords.data();

  assert(winCoords.size() % 3 == 0);

  for(int i = 0; i < winCoords.size(); i += 3)
  {
    tris_in++;

    int4 minwin, maxwin;
    MinMax(tri, minwin, maxwin);

    float4 invw(1.0f / homog[0].w, 1.0f / homog[1].w, 1.0f / homog[2].w, 0.0f);
    float4 depth(homog[0].z * invw.x, homog[1].z / invw.y, homog[2].z / invw.z, 0.0f);

    assert(minwin.x >= 0 && maxwin.x <= (int)w);
    assert(minwin.y >= 0 && maxwin.y <= (int)h);

    MICROPROFILE_SCOPEI("rasterizer", "TriLoop", MP_BLUE);

    for(int y = minwin.y; y < maxwin.y; y++)
    {
      for(int x = minwin.x; x < maxwin.x; x++)
      {
        int4 b = barycentric(tri, int4(x, y, 0, 0));

        if(b.x >= 0 && b.y >= 0 && b.z >= 0)
        {
          // normalise the barycentrics
          float4 n = float4(float(b.x), float(b.y), float(b.z), float(b.w));
          n.x /= n.w;
          n.y /= n.w;
          n.z /= n.w;

          // calculate pixel depth
          float pixdepth = n.x * depth.x + n.y * depth.y + n.z * depth.z;

          // perspective correct with W
          n.x *= invw.x;
          n.y *= invw.y;
          n.z *= invw.z;

          float invlen = 1.0f / (n.x + n.y + n.z);
          n.x *= invlen;
          n.y *= invlen;
          n.z *= invlen;

          float4 pix = PixelShader(n, pixdepth, homog, UV, tex);

          bits[y * w * 4 + x * 4 + 2] = byte(pix.x);
          bits[y * w * 4 + x * 4 + 1] = byte(pix.y);
          bits[y * w * 4 + x * 4 + 0] = byte(pix.z);

          written++;
        }

        tested++;
      }
    }

    tri += 3;
    homog += 3;
    UV += 3 * 4;
  }

  MICROPROFILE_COUNTER_ADD("rasterizer/pixels/tested", tested);
  MICROPROFILE_COUNTER_ADD("rasterizer/pixels/written", written);
  MICROPROFILE_COUNTER_ADD("rasterizer/triangles/in", tris_in);
}
