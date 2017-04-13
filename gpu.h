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
  VkImage target;
  VkPipeline pipeline;
  VkDescriptorSet set;
};

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

struct VertexCacheEntry
{
  float4 position;
  float4 interps[8];
};