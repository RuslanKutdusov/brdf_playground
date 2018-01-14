#include "global.h"

cbuffer InstanceParams : register(b1)
{
	float4x4 BakeViewProj[6];
	float4 ConstLuminanceColor;
	float ConstLuminance;
	bool UseConstLuminance;
	float Scale;
	bool UseCubeTexture;
	uint CubeTextureIdx;
	uint TextureIdx;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float3 normal : NORMAL;
#if CUBEMAP_BAKE
	uint id : INSTANCE_ID;
#endif
};

struct GSOutput
{
	float4 pos : SV_Position;
	float2 uv : TEXCOORD0;
	float3 normal : NORMAL;
	uint rtIdx : SV_RenderTargetArrayIndex;
};


VSOutput vs_main(SdkMeshVertex input, uint id : SV_InstanceID)
{
	VSOutput output;

	const float scale = 100.0f;
#if CUBEMAP_BAKE
	output.pos = mul(float4(input.pos * scale, 1.0f), BakeViewProj[id]);
	output.id = id;
#else
	output.pos = mul(float4(input.pos * scale + ViewPos.xyz, 1.0f), ViewProjMatrix);
#endif
	output.normal = input.normal;

	return output;
}


#if CUBEMAP_BAKE
[maxvertexcount(3)] 
void gs_main(triangle VSOutput input[3], inout TriangleStream<GSOutput> TriStream) 
{
	GSOutput output;

	for (int i = 0; i < 3; i++)
	{
		output.pos = input[i].pos;
		output.normal = input[i].normal;
		output.rtIdx = input[i].id;
		TriStream.Append(output);
	}
	TriStream.RestartStrip();
}
#endif


#if CUBEMAP_BAKE
float4 ps_main(GSOutput input) : SV_Target
#else
float4 ps_main(VSOutput input) : SV_Target
#endif
{
	if (UseConstLuminance)
		return ConstLuminanceColor * ConstLuminance;
	else if (UseCubeTexture)
		return TexturesCube[CubeTextureIdx].Sample(LinearWrapSampler, input.normal) * Scale;
	else
	{
		const float PI = 3.1415926f;
		float uAngle = atan2(input.normal.x, input.normal.z) / (2.0f * PI);
		float u = uAngle >= 0.0f ? uAngle : uAngle + 1.0f;
		float v = acos(input.normal.y) / PI;
		return Textures2D[TextureIdx].SampleLevel(LinearClampSampler, float2(u, v), 0) * Scale;
	}
	return 0.0f;
}