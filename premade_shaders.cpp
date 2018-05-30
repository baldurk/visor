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

void mul4(const float *a, const float *b, float *out)
{
  for(size_t x = 0; x < 4; x++)
  {
    for(size_t y = 0; y < 4; y++)
    {
      out[x * 4 + y] = b[x * 4 + 0] * a[0 * 4 + y] + b[x * 4 + 1] * a[1 * 4 + y] +
                       b[x * 4 + 2] * a[2 * 4 + y] + b[x * 4 + 3] * a[3 * 4 + y];
    }
  }
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
  float u = dot(bary, float4(tri[0].interps[0].x, tri[1].interps[0].x, tri[2].interps[0].x, 0.0f));
  float v = dot(bary, float4(tri[0].interps[0].y, tri[1].interps[0].y, tri[2].interps[0].y, 0.0f));

  // skipping lighting because we don't have derivatives!

  VkImage tex = state.set->binds[1].data.imageInfo.imageView->image;

  out = sample_tex_wrapped(u, v, tex);
  out.w = 1.0f;
}

void sascha_uioverlay_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out)
{
  const float *pos = (const float *)(state.vbs[0].buffer->bytes + state.vbs[0].offset);
  const float *UV = pos + 2;
  const uint32_t *col = (const uint32_t *)(pos + 4);

  pos += 5 * vertexIndex;
  UV += 5 * vertexIndex;
  col += 5 * vertexIndex;

  const float *scale = (const float *)state.pushconsts;
  const float *translate = scale + 2;

  out.position.x = pos[0] * scale[0] + translate[0];
  out.position.y = pos[1] * scale[1] + translate[1];
  out.position.z = 0.0f;
  out.position.w = 1.0f;

  out.interps[0].x = UV[0];
  out.interps[0].y = UV[1];

  out.interps[1].x = float((col[0] & 0x000000ff) >> 0x00) / 255.0f;
  out.interps[1].y = float((col[0] & 0x0000ff00) >> 0x08) / 255.0f;
  out.interps[1].z = float((col[0] & 0x00ff0000) >> 0x10) / 255.0f;
  out.interps[1].w = float((col[0] & 0xff000000) >> 0x18) / 255.0f;
}

void sascha_uioverlay_fs(const GPUState &state, float pixdepth, const float4 &bary,
                         const VertexCacheEntry tri[3], float4 &out)
{
  float u = dot(bary, float4(tri[0].interps[0].x, tri[1].interps[0].x, tri[2].interps[0].x, 0.0f));
  float v = dot(bary, float4(tri[0].interps[0].y, tri[1].interps[0].y, tri[2].interps[0].y, 0.0f));

  VkImage tex = state.set->binds[0].data.imageInfo.imageView->image;

  out = sample_tex_wrapped(u, v, tex);

  out.x *= dot(bary, float4(tri[0].interps[1].x, tri[1].interps[1].x, tri[2].interps[1].x, 0.0f));
  out.y *= dot(bary, float4(tri[0].interps[1].y, tri[1].interps[1].y, tri[2].interps[1].y, 0.0f));
  out.z *= dot(bary, float4(tri[0].interps[1].z, tri[1].interps[1].z, tri[2].interps[1].z, 0.0f));
  out.w *= dot(bary, float4(tri[0].interps[1].w, tri[1].interps[1].w, tri[2].interps[1].w, 0.0f));
}

void sascha_texture_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out)
{
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

  // vec4 color = texture(samplerColor, inUV, inLodBias);
  float4 color = sample_tex_wrapped(u, v, tex);

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

void sascha_vulkanscene_mesh_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out)
{
  const float *inPos = (const float *)(state.vbs[0].buffer->bytes + state.vbs[0].offset);

  inPos += 11 * vertexIndex;

  const float *inNormal = inPos + 3;
  const float *inTexCoord = inNormal + 3;
  const float *inColor = inTexCoord + 2;

  const VkDescriptorBufferInfo &buf = state.set->binds[0].data.bufferInfo;
  const VkBuffer ubo = buf.buffer;
  VkDeviceSize offs = buf.offset;

  const float *projection = (const float *)(ubo->bytes + offs);
  const float *model = projection + 16;
  const float *normal = model + 16;
  const float *view = normal + 16;
  const float *lightpos = view + 16;

  float *outUV = out.interps[0].v;
  float *outNormal = out.interps[1].v;
  float *outColor = out.interps[2].v;
  float *outEyePos = out.interps[3].v;
  float *outLightVec = out.interps[4].v;

  // outUV = inTexCoord.st;
  memcpy(outUV, inTexCoord, 2 * sizeof(float));

  // outNormal = normalize(mat3(ubo.normal) * inNormal);
  for(int row = 0; row < 3; row++)
  {
    outNormal[row] = 0.0f;

    for(int col = 0; col < 3; col++)
      outNormal[row] += normal[col * 4 + row] * inNormal[col];
  }

  normalize3(outNormal);

  // outColor = inColor;
  memcpy(outColor, inColor, 3 * sizeof(float));

  // mat4 modelView = ubo.view * ubo.model;
  float modelView[16];
  mul4(view, model, modelView);

  // vec4 pos = modelView * inPos;
  float4 pos = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for(int row = 0; row < 4; row++)
  {
    for(int col = 0; col < 4; col++)
      pos.v[row] += modelView[col * 4 + row] * (col < 3 ? inPos[col] : 1.0f);
  }

  // gl_Position = ubo.projection * pos;
  out.position = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for(int row = 0; row < 4; row++)
  {
    for(int col = 0; col < 4; col++)
      out.position.v[row] += projection[col * 4 + row] * pos.v[col];
  }

  // outEyePos = vec3(modelView * pos);
  for(int row = 0; row < 3; row++)
  {
    outEyePos[row] = 0.0f;

    for(int col = 0; col < 4; col++)
      outEyePos[row] += modelView[col * 4 + row] * pos.v[col];
  }

  // vec4 lightPos = vec4(ubo.lightpos, 1.0) * modelView;
  float4 lightPos = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for(int row = 0; row < 3; row++)
  {
    for(int col = 0; col < 4; col++)
      lightPos.v[row] += modelView[row * 4 + col] * (col < 3 ? lightpos[col] : 1.0f);
  }

  // outLightVec = normalize(lightPos.xyz - outEyePos);
  outLightVec[0] = lightPos.x - outEyePos[0];
  outLightVec[1] = lightPos.y - outEyePos[1];
  outLightVec[2] = lightPos.z - outEyePos[2];

  normalize3(outLightVec);
}

static float specpart(float4 L, float4 N, float4 H)
{
  if(dot(N, L) > 0.0f)
    return powint<64>(clamp(dot(H, N), 0.0f, 1.0f));

  return 0.0f;
}

void sascha_vulkanscene_mesh_fs(const GPUState &state, float pixdepth, const float4 &bary,
                                const VertexCacheEntry tri[3], float4 &out)
{
  float4 inNormal;
  inNormal.x = dot(bary, float4(tri[0].interps[1].x, tri[1].interps[1].x, tri[2].interps[1].x, 0.0f));
  inNormal.y = dot(bary, float4(tri[0].interps[1].y, tri[1].interps[1].y, tri[2].interps[1].y, 0.0f));
  inNormal.z = dot(bary, float4(tri[0].interps[1].z, tri[1].interps[1].z, tri[2].interps[1].z, 0.0f));
  inNormal.w = 0.0f;

  float4 inColor;
  inColor.x = dot(bary, float4(tri[0].interps[2].x, tri[1].interps[2].x, tri[2].interps[2].x, 0.0f));
  inColor.y = dot(bary, float4(tri[0].interps[2].y, tri[1].interps[2].y, tri[2].interps[2].y, 0.0f));
  inColor.z = dot(bary, float4(tri[0].interps[2].z, tri[1].interps[2].z, tri[2].interps[2].z, 0.0f));
  inColor.w = 0.0f;

  float4 inEyePos;
  inEyePos.x = dot(bary, float4(tri[0].interps[3].x, tri[1].interps[3].x, tri[2].interps[3].x, 0.0f));
  inEyePos.y = dot(bary, float4(tri[0].interps[3].y, tri[1].interps[3].y, tri[2].interps[3].y, 0.0f));
  inEyePos.z = dot(bary, float4(tri[0].interps[3].z, tri[1].interps[3].z, tri[2].interps[3].z, 0.0f));
  inEyePos.w = 0.0f;

  float4 inLightVec;
  inLightVec.x =
      dot(bary, float4(tri[0].interps[4].x, tri[1].interps[4].x, tri[2].interps[4].x, 0.0f));
  inLightVec.y =
      dot(bary, float4(tri[0].interps[4].y, tri[1].interps[4].y, tri[2].interps[4].y, 0.0f));
  inLightVec.z =
      dot(bary, float4(tri[0].interps[4].z, tri[1].interps[4].z, tri[2].interps[4].z, 0.0f));
  inLightVec.w = 0.0f;

  // vec3 Eye = normalize(-inEyePos);
  float4 Eye = float4(-inEyePos.x, -inEyePos.y, -inEyePos.z, 0.0f);
  normalize3(Eye);

  // vec3 Reflected = normalize(reflect(-inLightVec, inNormal));
  //   with reflect(I, N) = I - 2.0 * dot(N, I) * N;
  // vec3 Reflected = normalize((-inLightVec) - 2.0 * dot(inNormal, -inLightVec) * inNormal);
  float4 Reflected;
  float mulResult =
      2.0f * dot(inNormal, float4(-inLightVec.x, -inLightVec.y, -inLightVec.z, -inLightVec.w));
  Reflected.x = -inLightVec.x - mulResult * inNormal.x;
  Reflected.y = -inLightVec.y - mulResult * inNormal.y;
  Reflected.z = -inLightVec.z - mulResult * inNormal.z;
  Reflected.w = 0.0f;
  normalize3(Reflected);

  // vec3 halfVec = normalize(inLightVec + inEyePos);
  float4 halfVec =
      float4(inLightVec.x + inEyePos.x, inLightVec.y + inEyePos.y, inLightVec.z + inEyePos.z, 0.0f);
  normalize3(halfVec);

  // float diff = clamp(dot(inLightVec, inNormal), 0.0, 1.0);
  float diff = clamp(dot(inLightVec, inNormal), 0.0f, 1.0f);

  // float spec = specpart(inLightVec, inNormal, halfVec);
  float spec = specpart(inLightVec, inNormal, halfVec);

  // float intensity = 0.1 + diff + spec;
  float intensity = 0.1f + diff + spec;

  // vec4 IAmbient = vec4(0.2, 0.2, 0.2, 1.0);
  float IAmbient = 0.2f;

  // vec4 IDiffuse = vec4(0.5, 0.5, 0.5, 0.5) * max(dot(inNormal, inLightVec), 0.0);
  float IDiffuse = 0.5f * std::max(dot(inNormal, inLightVec), 0.0f);

  const float shininess = 0.75f;

  // vec4 ISpecular = vec4(0.5, 0.5, 0.5, 1.0) * pow(max(dot(Reflected, Eye), 0.0), 2.0) *
  // shininess;
  float ISpecular = 0.5f * powint<2>(std::max(dot(Reflected, Eye), 0.0f)) * shininess;

  // outFragColor = vec4((IAmbient + IDiffuse) * vec4(inColor, 1.0) + ISpecular);
  out.x = (IAmbient + IDiffuse) * inColor.x + ISpecular;
  out.y = (IAmbient + IDiffuse) * inColor.y + ISpecular;
  out.z = (IAmbient + IDiffuse) * inColor.z + ISpecular;

  // Some manual saturation
  if(intensity > 0.95)
  {
    // outFragColor *= 2.25;
    out.x *= 2.25f;
    out.y *= 2.25f;
    out.z *= 2.25f;
  }
  else if(intensity < 0.15)
  {
    // outFragColor = vec4(0.1);
    out.x = out.y = out.z = 0.1f;
  }

  if(out.x > 1.0f)
    out.x = 1.0f;
  if(out.y > 1.0f)
    out.y = 1.0f;
  if(out.z > 1.0f)
    out.z = 1.0f;

  out.w = 1.0f;
}

void sascha_vulkanscene_logo_fs(const GPUState &state, float pixdepth, const float4 &bary,
                                const VertexCacheEntry tri[3], float4 &out)
{
  float4 inNormal;
  inNormal.x = dot(bary, float4(tri[0].interps[1].x, tri[1].interps[1].x, tri[2].interps[1].x, 0.0f));
  inNormal.y = dot(bary, float4(tri[0].interps[1].y, tri[1].interps[1].y, tri[2].interps[1].y, 0.0f));
  inNormal.z = dot(bary, float4(tri[0].interps[1].z, tri[1].interps[1].z, tri[2].interps[1].z, 0.0f));
  inNormal.w = 0.0f;

  float4 inColor;
  inColor.x = dot(bary, float4(tri[0].interps[2].x, tri[1].interps[2].x, tri[2].interps[2].x, 0.0f));
  inColor.y = dot(bary, float4(tri[0].interps[2].y, tri[1].interps[2].y, tri[2].interps[2].y, 0.0f));
  inColor.z = dot(bary, float4(tri[0].interps[2].z, tri[1].interps[2].z, tri[2].interps[2].z, 0.0f));
  inColor.w = 0.0f;

  float4 inEyePos;
  inEyePos.x = dot(bary, float4(tri[0].interps[3].x, tri[1].interps[3].x, tri[2].interps[3].x, 0.0f));
  inEyePos.y = dot(bary, float4(tri[0].interps[3].y, tri[1].interps[3].y, tri[2].interps[3].y, 0.0f));
  inEyePos.z = dot(bary, float4(tri[0].interps[3].z, tri[1].interps[3].z, tri[2].interps[3].z, 0.0f));
  inEyePos.w = 0.0f;

  float4 inLightVec;
  inLightVec.x =
      dot(bary, float4(tri[0].interps[4].x, tri[1].interps[4].x, tri[2].interps[4].x, 0.0f));
  inLightVec.y =
      dot(bary, float4(tri[0].interps[4].y, tri[1].interps[4].y, tri[2].interps[4].y, 0.0f));
  inLightVec.z =
      dot(bary, float4(tri[0].interps[4].z, tri[1].interps[4].z, tri[2].interps[4].z, 0.0f));
  inLightVec.w = 0.0f;

  // vec3 Eye = normalize(-inEyePos);
  float4 Eye = float4(-inEyePos.x, -inEyePos.y, -inEyePos.z, 0.0f);
  normalize3(Eye);

  // vec3 Reflected = normalize(reflect(-inLightVec, inNormal));
  //   with reflect(I, N) = I - 2.0 * dot(N, I) * N;
  // vec3 Reflected = normalize((-inLightVec) - 2.0 * dot(inNormal, -inLightVec) * inNormal);
  float4 Reflected;
  float mulResult =
      2.0f * dot(inNormal, float4(-inLightVec.x, -inLightVec.y, -inLightVec.z, -inLightVec.w));
  Reflected.x = -inLightVec.x - mulResult * inNormal.x;
  Reflected.y = -inLightVec.y - mulResult * inNormal.y;
  Reflected.z = -inLightVec.z - mulResult * inNormal.z;
  Reflected.w = 0.0f;
  normalize3(Reflected);

  // vec4 diff = vec4(inColor, 1.0) * max(dot(inNormal, inLightVec), 0.0);
  float4 diff;
  float dotmul = std::max(dot(inLightVec, inNormal), 0.0f);
  diff.x = inColor.x * dotmul;
  diff.y = inColor.y * dotmul;
  diff.z = inColor.z * dotmul;
  diff.w = 1.0f;

  // float shininess = 0.0;
  const float shininess = 0.0f;

  // vec4 spec = vec4(1.0, 1.0, 1.0, 1.0) * pow(max(dot(Reflected, Eye), 0.0), 2.5) * shininess;
  // shininess == 0.0 so cancels out
  float4 spec(0.0f, 0.0f, 0.0f, 0.0f);

  // outFragColor = diff + spec;
  out.x = diff.x;
  out.y = diff.y;
  out.z = diff.z;

  // outFragColor.a = 1.0;
  out.w = 1.0f;
}

void sascha_vulkanscene_skybox_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out)
{
  const float *inPos = (const float *)(state.vbs[0].buffer->bytes + state.vbs[0].offset);

  inPos += 11 * vertexIndex;

  const VkDescriptorBufferInfo &buf = state.set->binds[0].data.bufferInfo;
  const VkBuffer ubo = buf.buffer;
  VkDeviceSize offs = buf.offset;

  const float *projection = (const float *)(ubo->bytes + offs);
  const float *model = projection + 16;

  float *outUVW = out.interps[0].v;

  // outUVW = inPos;
  outUVW[0] = inPos[0];
  outUVW[1] = inPos[1];
  outUVW[2] = inPos[2];

  // gl_Position = ubo.projection * ubo.model * vec4(inPos.xyz, 1.0);
  float4 modelMul = float4(0.0f, 0.0f, 0.0f, 0.0f);
  for(int col = 0; col < 4; col++)
  {
    for(int row = 0; row < 4; row++)
      modelMul.v[row] += model[col * 4 + row] * (col < 3 ? inPos[col] : 1.0f);
  }

  out.position.x = out.position.y = out.position.z = out.position.w = 0.0f;
  for(int col = 0; col < 4; col++)
  {
    for(int row = 0; row < 4; row++)
      out.position.v[row] += projection[col * 4 + row] * modelMul.v[col];
  }
}

void sascha_vulkanscene_skybox_fs(const GPUState &state, float pixdepth, const float4 &bary,
                                  const VertexCacheEntry tri[3], float4 &out)
{
  VkImage tex = state.set->binds[1].data.imageInfo.imageView->image;

  float u = dot(bary, float4(tri[0].interps[0].x, tri[1].interps[0].x, tri[2].interps[0].x, 0.0f));
  float v = dot(bary, float4(tri[0].interps[0].y, tri[1].interps[0].y, tri[2].interps[0].y, 0.0f));
  float w = dot(bary, float4(tri[0].interps[0].z, tri[1].interps[0].z, tri[2].interps[0].z, 0.0f));

  out = sample_cube_wrapped(u, v, w, tex);
}

static std::map<uint32_t, Shader> premadeShaderMap;

void InitPremadeShaders()
{
  if(premadeShaderMap.empty())
  {
    // premadeShaderMap.insert(std::make_pair<uint32_t, Shader>(1381679250, (Shader)&vkcube_vs));
    premadeShaderMap.insert(std::make_pair<uint32_t, Shader>(843116546, (Shader)&vkcube_fs));
    premadeShaderMap.insert(
        std::make_pair<uint32_t, Shader>(3338599131, (Shader)&sascha_uioverlay_vs));
    premadeShaderMap.insert(std::make_pair<uint32_t, Shader>(945681360, (Shader)&sascha_uioverlay_fs));
    premadeShaderMap.insert(std::make_pair<uint32_t, Shader>(3054859395, (Shader)&sascha_texture_vs));
    premadeShaderMap.insert(std::make_pair<uint32_t, Shader>(3971494927, (Shader)&sascha_texture_fs));

    premadeShaderMap.insert(
        std::make_pair<uint32_t, Shader>(1653401353, (Shader)&sascha_vulkanscene_mesh_vs));
    premadeShaderMap.insert(
        std::make_pair<uint32_t, Shader>(1068398372, (Shader)&sascha_vulkanscene_mesh_fs));
    // logo vertex shader - pretty much identical to mesh shader but with hardcoded light position,
    // which is identical to the value in the UBO anyway
    premadeShaderMap.insert(
        std::make_pair<uint32_t, Shader>(685038778, (Shader)&sascha_vulkanscene_mesh_vs));
    premadeShaderMap.insert(
        std::make_pair<uint32_t, Shader>(4043689846, (Shader)&sascha_vulkanscene_logo_fs));
    premadeShaderMap.insert(
        std::make_pair<uint32_t, Shader>(3977102539, (Shader)&sascha_vulkanscene_skybox_vs));
    premadeShaderMap.insert(
        std::make_pair<uint32_t, Shader>(1118272716, (Shader)&sascha_vulkanscene_skybox_fs));
  }
}

Shader GetPremadeShader(uint32_t hash)
{
  return premadeShaderMap[hash];
}
