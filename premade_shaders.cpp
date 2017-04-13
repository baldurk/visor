#include "precompiled.h"
#include <map>
#include <utility>
#include "gpu.h"

static float dot(const float4 &a, const float4 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

void vkcube_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out)
{
  const VkDescriptorBufferInfo &buf = state.set->binds[0].data.bufferInfo;
  const VkBuffer ubo = buf.buffer;
  VkDeviceSize offs = buf.offset;

  const float *MVP = (const float *)(ubo->bytes + offs);
  const float *pos = MVP + 4 * 4;
  const float *UV = pos + 12 * 3 * 4;

  pos += 4 * vertexIndex;
  UV += 4 * vertexIndex;

  out.position.x = out.position.y = out.position.z = out.position.w = 0.0f;

  for(int row = 0; row < 4; row++)
  {
    for(int col = 0; col < 4; col++)
      out.position.v[row] += MVP[col * 4 + row] * pos[col];
  }

  memcpy(out.interps[0].v, UV, sizeof(float4));
}

void vkcube_fs(const GPUState &state, float pixdepth, const float4 &bary,
               const VertexCacheEntry tri[3], float4 &out)
{
  MICROPROFILE_SCOPEI("rasterizer", "PixelShader", MP_WHITE);

  float u = dot(bary, float4(tri[0].interps[0].x, tri[1].interps[0].x, tri[2].interps[0].x, 0.0f));
  float v = dot(bary, float4(tri[0].interps[0].y, tri[1].interps[0].y, tri[2].interps[0].y, 0.0f));

  VkImage tex = state.set->binds[1].data.imageInfo.imageView->image;

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

  byte *TL = &tex->pixels[(iv0 * tex->extent.width + iu0) * tex->bytesPerPixel];
  byte *TR = &tex->pixels[(iv0 * tex->extent.width + iu1) * tex->bytesPerPixel];
  byte *BL = &tex->pixels[(iv1 * tex->extent.width + iu0) * tex->bytesPerPixel];
  byte *BR = &tex->pixels[(iv1 * tex->extent.width + iu1) * tex->bytesPerPixel];

  float4 top;
  top.x = float(TL[0]) * (1.0f - fu) + float(TR[0]) * fu;
  top.y = float(TL[1]) * (1.0f - fu) + float(TR[1]) * fu;
  top.z = float(TL[2]) * (1.0f - fu) + float(TR[2]) * fu;

  float4 bottom;
  bottom.x = float(BL[0]) * (1.0f - fu) + float(BR[0]) * fu;
  bottom.y = float(BL[1]) * (1.0f - fu) + float(BR[1]) * fu;
  bottom.z = float(BL[2]) * (1.0f - fu) + float(BR[2]) * fu;

  out.x = (top.x * (1.0f - fv) + bottom.x * fv) / 255.0f;
  out.y = (top.y * (1.0f - fv) + bottom.y * fv) / 255.0f;
  out.z = (top.z * (1.0f - fv) + bottom.z * fv) / 255.0f;
  out.w = 1.0f;
}

void sascha_textoverlay_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out)
{
}

void sascha_textoverlay_fs(const GPUState &state, float pixdepth, const float4 &bary,
                           const VertexCacheEntry tri[3], float4 &out)
{
}

void sascha_texture_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out)
{
}

void sascha_texture_fs(const GPUState &state, float pixdepth, const float4 &bary,
                       const VertexCacheEntry tri[3], float4 &out)
{
}

static std::map<uint32_t, Shader> premadeShaderMap;

void InitPremadeShaders()
{
  if(premadeShaderMap.empty())
  {
    premadeShaderMap.insert(std::make_pair<uint32_t, Shader>(2469737040, (Shader)&vkcube_vs));
    premadeShaderMap.insert(std::make_pair<uint32_t, Shader>(676538074, (Shader)&vkcube_fs));
    premadeShaderMap.insert(
        std::make_pair<uint32_t, Shader>(3142150232, (Shader)&sascha_textoverlay_vs));
    premadeShaderMap.insert(
        std::make_pair<uint32_t, Shader>(4293881502, (Shader)&sascha_textoverlay_fs));
    premadeShaderMap.insert(std::make_pair<uint32_t, Shader>(3054859395, (Shader)&sascha_texture_vs));
    premadeShaderMap.insert(std::make_pair<uint32_t, Shader>(3971494927, (Shader)&sascha_texture_fs));
  }
}

Shader GetPremadeShader(uint32_t hash)
{
  return premadeShaderMap[hash];
}
