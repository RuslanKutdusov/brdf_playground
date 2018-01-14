#pragma pack_matrix(row_major)

cbuffer GlobalParams : register(b0)
{
	uint PrefilteredDiffuseEnvMap;
	uint BRDFLut;
	uint PrefilteredSpecularEnvMap;
	uint ShadowMap;
	uint EnvironmentMap;
	uint pad0;
	uint pad1;
	uint pad2;
	float4x4 ViewProjMatrix;
	float4 ViewPos;
	float4 LightDir;
	float4 LightIlluminance;
	float4x4 ShadowViewProjMatrix;
	float4x4 ShadowMatrix;
	uint SamplingType;
	uint FrameIdx;
	uint TotalSamples;
	uint SamplesInStep;
	uint SamplesProcessed;
	bool EnableDirectLight;
	bool EnableEnvEmitter;
	bool EnableShadow;
	bool EnableDiffuseBRDF;
	bool EnableSpecularBRDF;
	uint ScreenWidth;
	uint ScreenHeight;
};


struct SdkMeshVertex
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float2 tex : TEXTURE0;
	float3 tan : TANGENT;
	float3 binormal : BINORMAL;
};


Texture2D Textures2D[] : register(t0, space0);
TextureCube TexturesCube[] : register(t0, space1);
Texture2DArray<float4> Textures2DArray[] : register(t0, space2);
RWTexture2D<float4> RWTextures2D[] : register(u0, space3);
RWTexture2DArray<float4> RWTextures2DArray[] : register(u0, space4);
Buffer<float> Buffers[] : register(t0, space5);

SamplerState LinearWrapSampler : register(s0);
SamplerState LinearClampSampler : register(s1);
SamplerComparisonState CmpLinearSampler : register(s2);


float3 LinearToSRGB(float3 color)
{
	float3 low = 12.92f * color;
	float3 high = 1.055f * pow(color, 1.0f / 2.4f) - 0.055f;
	return (color <= 0.0031308f) ? low : high;
}