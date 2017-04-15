#include "precompiled.h"
#include <map>
#include <utility>
#include "gpu.h"

void transpose(const float *in, float *out)
{
  for(size_t x = 0; x < 4; x++)
    for(size_t y = 0; y < 4; y++)
      out[x * 4 + y] = in[y * 4 + x];
}

void inverse(const float *in, float *out)
{
  float a0 = in[0] * in[5] - in[1] * in[4];
  float a1 = in[0] * in[6] - in[2] * in[4];
  float a2 = in[0] * in[7] - in[3] * in[4];
  float a3 = in[1] * in[6] - in[2] * in[5];
  float a4 = in[1] * in[7] - in[3] * in[5];
  float a5 = in[2] * in[7] - in[3] * in[6];
  float b0 = in[8] * in[13] - in[9] * in[12];
  float b1 = in[8] * in[14] - in[10] * in[12];
  float b2 = in[8] * in[15] - in[11] * in[12];
  float b3 = in[9] * in[14] - in[10] * in[13];
  float b4 = in[9] * in[15] - in[11] * in[13];
  float b5 = in[10] * in[15] - in[11] * in[14];

  float det = a0 * b5 - a1 * b4 + a2 * b3 + a3 * b2 - a4 * b1 + a5 * b0;
  if(fabsf(det) > FLT_EPSILON)
  {
    out[0] = +in[5] * b5 - in[6] * b4 + in[7] * b3;
    out[4] = -in[4] * b5 + in[6] * b2 - in[7] * b1;
    out[8] = +in[4] * b4 - in[5] * b2 + in[7] * b0;
    out[12] = -in[4] * b3 + in[5] * b1 - in[6] * b0;
    out[1] = -in[1] * b5 + in[2] * b4 - in[3] * b3;
    out[5] = +in[0] * b5 - in[2] * b2 + in[3] * b1;
    out[9] = -in[0] * b4 + in[1] * b2 - in[3] * b0;
    out[13] = +in[0] * b3 - in[1] * b1 + in[2] * b0;
    out[2] = +in[13] * a5 - in[14] * a4 + in[15] * a3;
    out[6] = -in[12] * a5 + in[14] * a2 - in[15] * a1;
    out[10] = +in[12] * a4 - in[13] * a2 + in[15] * a0;
    out[14] = -in[12] * a3 + in[13] * a1 - in[14] * a0;
    out[3] = -in[9] * a5 + in[10] * a4 - in[11] * a3;
    out[7] = +in[8] * a5 - in[10] * a2 + in[11] * a1;
    out[11] = -in[8] * a4 + in[9] * a2 - in[11] * a0;
    out[15] = +in[8] * a3 - in[9] * a1 + in[10] * a0;

    float invDet = 1.0f / det;
    out[0] *= invDet;
    out[1] *= invDet;
    out[2] *= invDet;
    out[3] *= invDet;
    out[4] *= invDet;
    out[5] *= invDet;
    out[6] *= invDet;
    out[7] *= invDet;
    out[8] *= invDet;
    out[9] *= invDet;
    out[10] *= invDet;
    out[11] *= invDet;
    out[12] *= invDet;
    out[13] *= invDet;
    out[14] *= invDet;
    out[15] *= invDet;

    return;
  }

  memset(out, 0, sizeof(float) * 16);
}

static float dot(const float4 &a, const float4 &b)
{
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

static float clamp(const float f, const float lower, const float upper)
{
  if(f < lower)
    return lower;
  else if(f > upper)
    return upper;
  else
    return f;
}

template <int p>
static float powint(const float x)
{
  float res = 1.0f;
  for(int i = 0; i < p; i++)
    res *= x;

  return res;
}

float rsqrt(float number)
{
  float ret;
  _mm_store_ss(&ret, _mm_rsqrt_ss(_mm_load_ss(&number)));
  return ret;
}

static void normalize3(float *a)
{
  float invlen = rsqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  a[0] *= invlen;
  a[1] *= invlen;
  a[2] *= invlen;
}

static void normalize3(float4 &a)
{
  return normalize3(a.v);
}

MICROPROFILE_DEFINE(vkcube_vs, "premade_shaders", "vkcube_vs", MP_BLACK);
MICROPROFILE_DEFINE(sascha_textoverlay_vs, "premade_shaders", "sascha_textoverlay_vs", MP_BLACK);
MICROPROFILE_DEFINE(sascha_texture_vs, "premade_shaders", "sascha_texture_vs", MP_BLACK);

void vkcube_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out)
{
  MICROPROFILE_SCOPE(vkcube_vs);

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
  MICROPROFILE_SCOPE(sascha_textoverlay_vs);

  const float *pos = (const float *)(state.vbs[0].buffer->bytes + state.vbs[0].offset);
  const float *UV = (const float *)(state.vbs[1].buffer->bytes + state.vbs[1].offset);

  // pos and UV are packed together, the VB is duplicated for some reason
  UV += 2;

  pos += 4 * vertexIndex;
  UV += 4 * vertexIndex;

  out.position.x = pos[0];
  out.position.y = pos[1];
  out.position.z = 0.0f;
  out.position.w = 1.0f;

  out.interps[0].x = UV[0];
  out.interps[0].y = UV[1];
}

void sascha_textoverlay_fs(const GPUState &state, float pixdepth, const float4 &bary,
                           const VertexCacheEntry tri[3], float4 &out)
{
  float u = dot(bary, float4(tri[0].interps[0].x, tri[1].interps[0].x, tri[2].interps[0].x, 0.0f));
  float v = dot(bary, float4(tri[0].interps[0].y, tri[1].interps[0].y, tri[2].interps[0].y, 0.0f));

  VkImage tex = state.set->binds[0].data.imageInfo.imageView->image;

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

  float top;
  top = float(TL[0]) * (1.0f - fu) + float(TR[0]) * fu;

  float bottom;
  bottom = float(BL[0]) * (1.0f - fu) + float(BR[0]) * fu;

  out.x = out.y = out.z = (top * (1.0f - fv) + bottom * fv) / 255.0f;
  out.w = 1.0f;
}

void sascha_texture_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out)
{
  MICROPROFILE_SCOPE(sascha_texture_vs);

  const float *inPos = (const float *)(state.vbs[0].buffer->bytes + state.vbs[0].offset);

  inPos += 8 * vertexIndex;

  const float *inUV = inPos + 3;
  const float *inNormal = inUV + 2;

  const VkDescriptorBufferInfo &buf = state.set->binds[0].data.bufferInfo;
  const VkBuffer ubo = buf.buffer;
  VkDeviceSize offs = buf.offset;

  const float *projection = (const float *)(ubo->bytes + offs);
  const float *model = projection + 16;
  const float *viewPos = model + 16;
  const float *lodBias = viewPos + 4;

  // outUV = inUV;
  out.interps[0].x = inUV[0];
  out.interps[0].y = inUV[1];

  // outLodBias = ubo.lodBias;
  out.interps[1].x = lodBias[0];

  // vec3 worldPos = vec3(ubo.model * vec4(inPos, 1.0));
  float4 worldPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for(int row = 0; row < 4; row++)
  {
    for(int col = 0; col < 4; col++)
      worldPos.v[row] += model[col * 4 + row] * (col < 3 ? inPos[col] : 1.0f);
  }

  // gl_Position = ubo.projection * ubo.model * vec4(inPos.xyz, 1.0);
  out.position.x = out.position.y = out.position.z = out.position.w = 0.0f;

  for(int row = 0; row < 4; row++)
  {
    for(int col = 0; col < 4; col++)
      out.position.v[row] += projection[col * 4 + row] * worldPos.v[col];
  }

  // vec4 pos = ubo.model * vec4(inPos, 1.0);
  float4 pos = worldPos;

  // outNormal = mat3(inverse(transpose(ubo.model))) * inNormal;
  float transpMat[16];
  transpose(model, transpMat);
  float invMat[16];
  inverse(transpMat, invMat);
  out.interps[2] = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for(int row = 0; row < 3; row++)
  {
    for(int col = 0; col < 3; col++)
      out.interps[2].v[row] += invMat[col * 4 + row] * inNormal[col];
  }

  // vec3 lightPos = vec3(0.0);
  float4 lightPos = float4(0.0f, 0.0f, 0.0f, 0.0f);

  // vec3 lPos = mat3(ubo.model) * lightPos.xyz;
  float4 lPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for(int row = 0; row < 3; row++)
  {
    for(int col = 0; col < 3; col++)
      lPos.v[row] += model[col * 4 + row] * lightPos.v[col];
  }

  // outLightVec = lPos - pos.xyz;
  out.interps[4].x = lPos.x - pos.x;
  out.interps[4].y = lPos.y - pos.y;
  out.interps[4].z = lPos.z - pos.z;

  // outViewVec = ubo.viewPos.xyz - pos.xyz;
  out.interps[3].x = viewPos[0] - pos.x;
  out.interps[3].y = viewPos[1] - pos.y;
  out.interps[3].z = viewPos[2] - pos.z;
}

void sascha_texture_fs(const GPUState &state, float pixdepth, const float4 &bary,
                       const VertexCacheEntry tri[3], float4 &out)
{
  VkImage tex = state.set->binds[1].data.imageInfo.imageView->image;

  float u = dot(bary, float4(tri[0].interps[0].x, tri[1].interps[0].x, tri[2].interps[0].x, 0.0f));
  float v = dot(bary, float4(tri[0].interps[0].y, tri[1].interps[0].y, tri[2].interps[0].y, 0.0f));

  float ou = u;
  float ov = v;

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
  top.w = float(TL[3]) * (1.0f - fu) + float(TR[3]) * fu;

  float4 bottom;
  bottom.x = float(BL[0]) * (1.0f - fu) + float(BR[0]) * fu;
  bottom.y = float(BL[1]) * (1.0f - fu) + float(BR[1]) * fu;
  bottom.z = float(BL[2]) * (1.0f - fu) + float(BR[2]) * fu;
  bottom.w = float(BL[3]) * (1.0f - fu) + float(BR[3]) * fu;

  // vec4 color = texture(samplerColor, inUV, inLodBias);
  float4 color;
  color.x = (top.x * (1.0f - fv) + bottom.x * fv) / 255.0f;
  color.y = (top.y * (1.0f - fv) + bottom.y * fv) / 255.0f;
  color.z = (top.z * (1.0f - fv) + bottom.z * fv) / 255.0f;
  color.w = (top.w * (1.0f - fv) + bottom.w * fv) / 255.0f;

  // vec3 N = normalize(inNormal);
  float4 N;
  N.x = dot(bary, float4(tri[0].interps[2].x, tri[1].interps[2].x, tri[2].interps[2].x, 0.0f));
  N.y = dot(bary, float4(tri[0].interps[2].y, tri[1].interps[2].y, tri[2].interps[2].y, 0.0f));
  N.z = dot(bary, float4(tri[0].interps[2].z, tri[1].interps[2].z, tri[2].interps[2].z, 0.0f));
  N.w = 0.0f;
  normalize3(N);

  // vec3 L = normalize(inLightVec);
  float4 L;
  L.x = dot(bary, float4(tri[0].interps[4].x, tri[1].interps[4].x, tri[2].interps[4].x, 0.0f));
  L.y = dot(bary, float4(tri[0].interps[4].y, tri[1].interps[4].y, tri[2].interps[4].y, 0.0f));
  L.z = dot(bary, float4(tri[0].interps[4].z, tri[1].interps[4].z, tri[2].interps[4].z, 0.0f));
  L.w = 0.0f;
  normalize3(L);

  // vec3 V = normalize(inViewVec);
  float4 V;
  V.x = dot(bary, float4(tri[0].interps[3].x, tri[1].interps[3].x, tri[2].interps[3].x, 0.0f));
  V.y = dot(bary, float4(tri[0].interps[3].y, tri[1].interps[3].y, tri[2].interps[3].y, 0.0f));
  V.z = dot(bary, float4(tri[0].interps[3].z, tri[1].interps[3].z, tri[2].interps[3].z, 0.0f));
  V.w = 0.0f;
  normalize3(V);

  // vec3 R = reflect(-L, N);
  //   with reflect(I, N) = I - 2.0 * dot(N, I) * N;
  // vec3 R = (-L) - 2.0 * dot(N, -L) * N;
  float4 R;
  float mulResult = 2.0f * dot(N, float4(-L.x, -L.y, -L.z, -L.w));
  R.x = (-L.x) - mulResult * N.x;
  R.y = (-L.y) - mulResult * N.y;
  R.z = (-L.z) - mulResult * N.z;
  R.w = 0.0f;

  // vec3 diffuse = max(dot(N, L), 0.0) * vec3(1.0);
  float diffuse = std::max(dot(N, L), 0.0f);

  // float specular = pow(max(dot(R, V), 0.0), 16.0) * color.a;
  float specular = powint<16>(std::max(dot(R, V), 0.0f)) * color.w;

  // outFragColor = vec4(diffuse * color.rgb + specular, 1.0);
  out.x = std::min(1.0f, diffuse * color.x + specular);
  out.y = std::min(1.0f, diffuse * color.y + specular);
  out.z = std::min(1.0f, diffuse * color.z + specular);
  out.w = 1.0f;
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
