#pragma once

struct GPUState
{
  struct
  {
    VkBuffer buffer;
    VkDeviceSize offset;
    VkIndexType indexType;
  } ib;
  struct
  {
    VkBuffer buffer;
    VkDeviceSize offset;
  } vbs[4];
  VkViewport view;
  VkImage col[8];
  VkImage depth;
  VkPipeline pipeline;
  VkDescriptorSet set;
  byte pushconsts[128];
};

__declspec(align(16)) struct int4
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

__declspec(align(16)) struct float4
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

struct VertexCacheEntry
{
  float4 position;
  float4 interps[8];
};

void ClearTarget(VkImage target, const VkClearColorValue &col);
void ClearTarget(VkImage target, const VkClearDepthStencilValue &col);
void DrawTriangles(const GPUState &state, int numVerts, uint32_t first, bool indexed);

void InitTextureCache();
extern "C" __declspec(dllexport) void sample_tex_wrapped(float u, float v, VkImage tex,
                                                         VkDeviceSize byteOffs, float4 &out);
extern "C" __declspec(dllexport) void sample_cube_wrapped(float x, float y, float z, VkImage tex,
                                                          float4 &out);

void InitRasterThreads();
void ShutdownRasterThreads();