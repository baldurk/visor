#include "precompiled.h"
#include "gpu.h"

struct TextureCacheEntry
{
  TextureCacheEntry *prev = NULL;
  TextureCacheEntry *next = NULL;
  int TLx = -1, TLy = -1;
  VkDeviceSize byteOffs = 0;
  VkImage tex = VK_NULL_HANDLE;
  float4 pixels[4][4];
} tcache_data[8];

TextureCacheEntry *tcache_head = NULL;

void InitTextureCache()
{
  tcache_head = &tcache_data[0];

  tcache_data[0].next = &tcache_data[1];

  for(int i = 1; i < 7; i++)
  {
    tcache_data[i].prev = &tcache_data[i - 1];
    tcache_data[i].next = &tcache_data[i + 1];
  }

  tcache_data[7].prev = &tcache_data[6];
}

float4 &CacheCoord(VkImage tex, VkDeviceSize byteOffs, int x, int y)
{
  // get the top-left of the 4x4 quad containin (x,y)
  int TLx = x & ~0x3;
  int TLy = y & ~0x3;

  TextureCacheEntry *tcache = tcache_head;
  while(tcache)
  {
    if(tcache->tex == tex && tcache->byteOffs == byteOffs && tcache->TLx == TLx && tcache->TLy == TLy)
    {
      MICROPROFILE_COUNTER_ADD("tcache/hits", 1);

      // move to the head of the cache list if it's not there already
      if(tcache->prev)
      {
        // remove from list
        tcache->prev->next = tcache->next;
        if(tcache->next)
          tcache->next->prev = tcache->prev;

        // move to head
        tcache_head->prev = tcache;
        tcache->prev = NULL;
        tcache->next = tcache_head;
        tcache_head = tcache;
      }

      return tcache->pixels[y & 0x3][x & 0x3];
    }

    // if we hit the last item, break
    if(!tcache->next)
      break;

    tcache = tcache->next;
  }

  MICROPROFILE_COUNTER_ADD("tcache/misses", 1);

  // tcache points to the last item in the list since we just iterated over all of it above.

  // remove from the end of the list
  tcache->prev->next = NULL;

  // move to head
  tcache_head->prev = tcache;
  tcache->prev = NULL;
  tcache->next = tcache_head;
  tcache_head = tcache;

  // pull in contents from texture
  tcache->tex = tex;
  tcache->byteOffs = byteOffs;
  tcache->TLx = TLx;
  tcache->TLy = TLy;

  byte *base = tex->pixels + byteOffs;

  const uint32_t bpp = tex->bytesPerPixel;

  for(int row = 0; row < 4; row++)
  {
    byte *rowbase = base + ((TLy + row) * tex->extent.width + TLx) * bpp;

    for(int col = 0; col < 4; col++)
    {
      tcache->pixels[row][col] =
          float4(float(rowbase[col * bpp + 0]) / 255.0f, float(rowbase[col * bpp + 1]) / 255.0f,
                 float(rowbase[col * bpp + 2]) / 255.0f, float(rowbase[col * bpp + 3]) / 255.0f);
    }
  }

  return tcache->pixels[y & 0x3][x & 0x3];
}

float4 sample_tex_wrapped(float u, float v, VkImage tex, VkDeviceSize byteOffs)
{
  u = fmodf(u, 1.0f);
  v = fmodf(v, 1.0f);

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
  float inv_fu = 1.0f - fu;
  float inv_fv = 1.0f - fv;

  const float4 &TL = CacheCoord(tex, byteOffs, iu0, iv0);
  const float4 &TR = CacheCoord(tex, byteOffs, iu1, iv0);
  const float4 &BL = CacheCoord(tex, byteOffs, iu0, iv1);
  const float4 &BR = CacheCoord(tex, byteOffs, iu1, iv1);

  float4 top;
  top.x = TL.x * inv_fu + TR.x * fu;
  top.y = TL.y * inv_fu + TR.y * fu;
  top.z = TL.z * inv_fu + TR.z * fu;
  top.w = TL.w * inv_fu + TR.w * fu;

  float4 bottom;
  bottom.x = BL.x * inv_fu + BR.x * fu;
  bottom.y = BL.y * inv_fu + BR.y * fu;
  bottom.z = BL.z * inv_fu + BR.z * fu;
  bottom.w = BL.w * inv_fu + BR.w * fu;

  float4 color;
  color.x = top.x * inv_fv + bottom.x * fv;
  color.y = top.y * inv_fv + bottom.y * fv;
  color.z = top.z * inv_fv + bottom.z * fv;
  color.w = top.w * inv_fv + bottom.w * fv;

  return color;
}

float4 sample_cube_wrapped(float x, float y, float z, VkImage tex)
{
  float ax = abs(x);
  float ay = abs(y);
  float az = abs(z);

  bool px = x > 0.0f;
  bool py = y > 0.0f;
  bool pz = z > 0.0f;

  float axis, u, v;
  VkDeviceSize offset = 0;

  // X+
  if(px && ax >= ay && ax >= az)
  {
    axis = ax;
    u = -z;
    v = -y;
    offset = CalcSubresourceByteOffset(tex, 0, 0);
  }
  // X-
  if(!px && ax >= ay && ax >= az)
  {
    axis = ax;
    u = z;
    v = -y;
    offset = CalcSubresourceByteOffset(tex, 0, 1);
  }
  // Y+
  if(py && ay >= ax && ay >= az)
  {
    axis = ay;
    u = x;
    v = z;
    offset = CalcSubresourceByteOffset(tex, 0, 2);
  }
  // Y-
  if(!py && ay >= ax && ay >= az)
  {
    axis = ay;
    u = x;
    v = -z;
    offset = CalcSubresourceByteOffset(tex, 0, 3);
  }
  // Z+
  if(pz && az >= ax && az >= ay)
  {
    axis = az;
    u = x;
    v = -y;
    offset = CalcSubresourceByteOffset(tex, 0, 4);
  }
  // Z-
  if(!pz && az >= ax && az >= ay)
  {
    axis = az;
    u = -x;
    v = -y;
    offset = CalcSubresourceByteOffset(tex, 0, 5);
  }

  return sample_tex_wrapped(0.5f * (u / axis + 1.0f), 0.5f * (v / axis + 1.0f), tex, offset);
}
