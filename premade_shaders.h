#pragma once

void vkcube_vs(const GPUState &state, uint32_t vertexIndex, VertexCacheEntry &out);
void vkcube_fs(const GPUState &state, float pixdepth, const float4 &bary,
               const VertexCacheEntry tri[3], float4 &out);
