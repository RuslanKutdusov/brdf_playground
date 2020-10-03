#include "global.h"
#include "lighting.h"

cbuffer LineParams : register(b1)
{
	float3 start;
	float3 end;
	float3 color;
};


float4 vs_main(uint vid : SV_VertexID) : SV_Position
{
	float3 pos = vid == 0 ? start : end;
	return mul(float4(pos, 1), ViewProjMatrix);
}


float4 ps_main() : SV_Target0
{
	return float4(color, 1.0f);
}