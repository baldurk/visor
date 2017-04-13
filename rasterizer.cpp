#include "precompiled.h"
#include "gpu.h"

static std::vector<VertexCacheEntry> ShadeVerts(const GPUState &state, int numVerts, uint32_t first)
{
  MICROPROFILE_SCOPEI("rasterizer", "ShadeVerts", MP_KHAKI);

  std::vector<VertexCacheEntry> ret;

  VertexCacheEntry tri[4];

  int a = 0, b = 1, c = 2;
  if(state.pipeline->frontFace == VK_FRONT_FACE_CLOCKWISE)
    std::swap(a, c);

  if(state.pipeline->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
  {
    VertexCacheEntry vert;

    // only handle whole triangles
    int lastVert = numVerts - 3;
    for(int v = 0; v <= lastVert; v += 3)
    {
      state.pipeline->vs(state, v + 0 + first, tri[0]);
      state.pipeline->vs(state, v + 1 + first, tri[1]);
      state.pipeline->vs(state, v + 2 + first, tri[2]);

      ret.push_back(tri[a]);
      ret.push_back(tri[b]);
      ret.push_back(tri[c]);
    }
  }
  else if(state.pipeline->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
  {
    assert(numVerts >= 3);

    // strip order to preserve winding order is:
    // N+0, N+1, N+2
    // N+2, N+1, N+3
    // N+2, N+3, N+4
    // N+4, N+5, N+6
    // ...
    //
    // So each pair of triangles forms the same pattern, we alternate between one and the other.
    // Bear in mind that the strip might end after the first half of a pair so we can't just shade
    // all 4 verts:
    //
    // N+0, N+1, N+2
    // N+2, N+1, N+3
    // M = N+2
    // M+0, M+1, M+2
    // M+2, M+1, M+3
    // S = M+2
    // S+0, S+1, S+2
    // S+2, S+1, S+3

    int a = 0, b = 1, c = 2;
    int a2 = 2, b2 = 1, c2 = 3;
    if(state.pipeline->frontFace == VK_FRONT_FACE_CLOCKWISE)
    {
      std::swap(a, c);
      std::swap(a2, c2);
    }

    // do the first one separately when we have to emit a whole triangle
    uint32_t vertexIndex = first;

    state.pipeline->vs(state, vertexIndex, tri[0]);
    vertexIndex++;
    state.pipeline->vs(state, vertexIndex, tri[1]);
    vertexIndex++;
    state.pipeline->vs(state, vertexIndex, tri[2]);
    vertexIndex++;

    ret.push_back(tri[a]);
    ret.push_back(tri[b]);
    ret.push_back(tri[c]);

    numVerts -= 3;

    if(numVerts > 0)
    {
      state.pipeline->vs(state, vertexIndex, tri[3]);
      vertexIndex++;
      numVerts--;

      ret.push_back(tri[a2]);
      ret.push_back(tri[b2]);
      ret.push_back(tri[c2]);
    }

    while(numVerts > 0)
    {
      // pull in two re-used verts from previous run.
      // See above:
      //
      // M = N+2
      // M+0, M+1, M+2
      //
      // so M+0 = N+2, M+1 = N+3
      tri[0] = tri[2];
      tri[1] = tri[3];

      state.pipeline->vs(state, vertexIndex, tri[2]);
      vertexIndex++;
      numVerts--;

      ret.push_back(tri[a]);
      ret.push_back(tri[b]);
      ret.push_back(tri[c]);

      if(numVerts > 0)
      {
        state.pipeline->vs(state, vertexIndex, tri[3]);
        vertexIndex++;
        numVerts--;

        ret.push_back(tri[a2]);
        ret.push_back(tri[b2]);
        ret.push_back(tri[c2]);
      }
    }
  }
  else
  {
    printf("Unsupported primitive topology!\n");
  }

  return ret;
}

static std::vector<int4> ToWindow(uint32_t w, uint32_t h, const std::vector<VertexCacheEntry> &pos)
{
  MICROPROFILE_SCOPEI("rasterizer", "ToWindow", MP_GREEN);

  std::vector<int4> ret;

  for(const VertexCacheEntry &v : pos)
  {
    int4 win(0, 0, 0, 0);

    win.x = int((v.position.x / v.position.w + 1.0f) * 0.5f * w);
    win.y = int((v.position.y * -1.0f / v.position.w + 1.0f) * 0.5f * h);

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

void DrawTriangles(const GPUState &state, int numVerts, uint32_t first)
{
  byte *bits = state.target->pixels;
  const uint32_t w = state.target->extent.width;
  const uint32_t h = state.target->extent.height;

  std::vector<VertexCacheEntry> shadedVerts = ShadeVerts(state, numVerts, first);

  std::vector<int4> winCoords = ToWindow(w, h, shadedVerts);

  int written = 0, tested = 0, tris_in = 0;

  const int4 *tri = winCoords.data();
  const VertexCacheEntry *vsout = shadedVerts.data();

  assert(winCoords.size() % 3 == 0);

  for(int i = 0; i < winCoords.size(); i += 3)
  {
    tris_in++;

    int4 minwin, maxwin;
    MinMax(tri, minwin, maxwin);

    float4 invw(1.0f / vsout[0].position.w, 1.0f / vsout[1].position.w, 1.0f / vsout[2].position.w,
                0.0f);
    float4 depth(vsout[0].position.z * invw.x, vsout[1].position.z / invw.y,
                 vsout[2].position.z / invw.z, 0.0f);

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

          float4 pix;
          state.pipeline->fs(state, pixdepth, n, vsout, pix);

          bits[y * w * 4 + x * 4 + 2] = byte(pix.x * 255.0f);
          bits[y * w * 4 + x * 4 + 1] = byte(pix.y * 255.0f);
          bits[y * w * 4 + x * 4 + 0] = byte(pix.z * 255.0f);

          written++;
        }

        tested++;
      }
    }

    tri += 3;
    vsout += 3;
  }

  MICROPROFILE_COUNTER_ADD("rasterizer/pixels/tested", tested);
  MICROPROFILE_COUNTER_ADD("rasterizer/pixels/written", written);
  MICROPROFILE_COUNTER_ADD("rasterizer/triangles/in", tris_in);
}
