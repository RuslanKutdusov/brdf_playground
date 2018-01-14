#include "global.h"
#include "lighting.h"

cbuffer Constants : register(b1)
{
	uint sourceTexIdx;
	uint destTexIdx;
	uint mipIdx;
};

[numthreads(32, 32, 1)] 
void cs_main(uint2 id : SV_DispatchThreadID, uint3 groupId : SV_GroupID) 
{
	RWTexture2DArray<float4> dst = RWTextures2DArray[destTexIdx];
	Texture2DArray<float4> src = Textures2DArray[sourceTexIdx];

	uint faceIndex = groupId.z;
	uint width, height, arraySize;
	dst.GetDimensions(width, height, arraySize);
	if (id.x >= width || id.y >= height)
		return;

	uint2 srcTexelBase = id * 2;
	float4 p0 = src.Load(uint4(srcTexelBase.x + 0, srcTexelBase.y + 0, faceIndex, mipIdx - 1));
	float4 p1 = src.Load(uint4(srcTexelBase.x + 0, srcTexelBase.y + 1, faceIndex, mipIdx - 1));
	float4 p2 = src.Load(uint4(srcTexelBase.x + 1, srcTexelBase.y + 0, faceIndex, mipIdx - 1));
	float4 p3 = src.Load(uint4(srcTexelBase.x + 1, srcTexelBase.y + 1, faceIndex, mipIdx - 1));
	dst[uint3(id.xy, faceIndex)] = (p0 + p1 + p2 + p3) / 4.0f;
}