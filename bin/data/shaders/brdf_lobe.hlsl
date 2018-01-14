#include "global.h"
#include "lighting.h"

cbuffer BrdfParams : register(b1)
{
	float Roughness;
	float Metalness;
	float Reflectance;
	float Illuminance;
	float3 Color;
};


struct VsOutput
{
	float4 pos : SV_Position;
	float3 worldPos : WORLD_POS;
};


VsOutput vs_main(float3 meshPos : POSITION)
{
	float3 N = float3(0.0f, 1.0f, 0.0f);
	float3 V = meshPos;
	float3 baseColor = 1.0f;
	MaterialData materialData = InitMaterialData(kMaterialSimple, Metalness, Roughness, Reflectance, baseColor);
	float val = CalcDirectLight(N, LightDir.xyz, V, materialData).r * Illuminance;

	float3 worldPos = meshPos * val;

	VsOutput output;
	output.pos = mul(float4(worldPos, 1), ViewProjMatrix);
	output.worldPos = worldPos;
	return output;
}


float4 ps_main(VsOutput input) : SV_Target0
{
	float3 worldPos = input.worldPos;
	float3 normal = normalize(cross(ddx(worldPos), ddy(worldPos)));
	float3 view = normalize(ViewPos.xyz - worldPos);
	float3 color = dot(normal, view) * Color;
	return LinearToSRGB(color).rgbb;
}