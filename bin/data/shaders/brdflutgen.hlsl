#include "global.h"
#include "lighting.h"

cbuffer Constants : register(b1)
{
	uint BRDFLutUAVIdx;
};

[numthreads(32, 32, 1)] 
void cs_main(uint2 id : SV_DispatchThreadID, uint3 groupId : SV_GroupID) 
{
	RWTexture2D<float4> BRDFLutUAV = RWTextures2D[BRDFLutUAVIdx];

	uint width, height;
	BRDFLutUAV.GetDimensions(width, height);
	if (id.x >= width || id.y >= height)
		return;

	float roughness = (float)id.x / (float)(width - 1);
	float NoV = (float)id.y / (float)(height - 1);

	uint2 random = RandVector_v2(id.xy);
	float2 lut = GenerateBRDFLut(roughness, NoV, random);

	BRDFLutUAV[id.xy] = float4(lut, 0.0, 0.0);
}