#include "global.h"

cbuffer ConstBuffer : register(b1)
{
	float4x4 ProjectionMatrix;
	uint TextureIndex;
};


struct VertexInput
{
	float2 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};


struct VertexOutput
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};


VertexOutput vs_main(VertexInput input)
{
	VertexOutput output;
	output.pos = mul(float4(input.pos.xy, 0.0f, 1.0f), ProjectionMatrix);
	output.col = input.col;
	output.uv = input.uv;
	return output;
}


float4 ps_main(VertexOutput input) : SV_Target
{
	return input.col * Textures2D[TextureIndex].Sample(LinearWrapSampler, input.uv);
}