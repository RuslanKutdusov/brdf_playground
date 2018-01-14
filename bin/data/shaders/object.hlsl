#include "global.h"
#include "lighting.h"

cbuffer InstanceParams : register(b1)
{
	struct
	{
		float4x4 WorldMatrix;
		float Metalness;
		float Roughness;
		float Reflectance;
		float4 BaseColor;
		uint MaterialType;
		uint3 padding;
	} InstanceData[128];
};

cbuffer MaterialConstBuf : register(b2)
{
	uint BaseColorTexture;
	uint NormalTexture;
	uint RoughnessTexture;
	uint MetalnessTexture;
	uint AoTexture;
	uint MerlBRDF;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float3 worldPos : WORLDPOS;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float2 uv : UV;
	uint id : INSTANCE_ID;
};


VSOutput vs_main(SdkMeshVertex input, uint id : SV_InstanceID)
{
	VSOutput output;

	float4 worldPos = mul(float4(input.pos, 1.0f), InstanceData[id].WorldMatrix);
#if SHADOW_PASS
	output.pos = mul(worldPos, ShadowViewProjMatrix);
#else
	output.pos = mul(worldPos, ViewProjMatrix);
#endif

	output.worldPos = worldPos.xyz;
	output.normal = mul(float4(input.normal, 0.0f), InstanceData[id].WorldMatrix).xyz;
	output.tangent = mul(float4(input.tan, 0.0f), InstanceData[id].WorldMatrix).xyz;
	output.binormal = mul(float4(input.binormal, 0.0f), InstanceData[id].WorldMatrix).xyz;
	output.uv = input.tex;

	output.id = id;

	return output;
}


struct PSOutput
{
	float4 directLight : SV_Target0;
	float4 indirectLight : SV_Target1;
};


PSOutput ps_main(VSOutput input)
{
	float4 pixelPos = input.pos;
	float3 normal = normalize(input.normal);
	float3 tangent = normalize(input.tangent);
	float3 binormal = normalize(input.binormal);  // cross( normal, tangent );
	float3 view = normalize(ViewPos.xyz - input.worldPos);

	float metalness = 0.0f;
	float roughness = 0.0f;
	float reflectance = 1.0f;
	float3 baseColor = 1.0f;
	float ao = 1.0f;
	if (InstanceData[input.id].MaterialType == kMaterialTexture)
	{
		if (BaseColorTexture != ~0u)
			baseColor = Textures2D[BaseColorTexture].Sample(LinearWrapSampler, input.uv).rgb;
		if (MetalnessTexture != ~0u)
			metalness = Textures2D[MetalnessTexture].Sample(LinearWrapSampler, input.uv).r;
		if (RoughnessTexture != ~0u)
			roughness = Textures2D[RoughnessTexture].Sample(LinearWrapSampler, input.uv).r;
		if (NormalTexture != ~0u)
		{
			float2 normalTSxy = Textures2D[NormalTexture].Sample(LinearWrapSampler, input.uv).rg * 2.0f - 1.0f;
			float3 normalTS = float3(normalTSxy, sqrt(1.0f - saturate(dot(normalTSxy.xy, normalTSxy.xy))));
			normal = normalize(mul(normalTS, float3x3(tangent, binormal, normal)));
		}
		if (AoTexture != ~0u)
			ao = Textures2D[AoTexture].Sample(LinearWrapSampler, input.uv).r;
	}
	else
	{
		baseColor = InstanceData[input.id].BaseColor.rgb;
		metalness = InstanceData[input.id].Metalness;
		roughness = InstanceData[input.id].Roughness;
		reflectance = InstanceData[input.id].Reflectance;
	}

	MaterialData materialData = InitMaterialData(InstanceData[input.id].MaterialType, metalness, roughness, reflectance, baseColor);

	PSOutput output = (PSOutput)0;
	uint2 random = RandVector_v2(pixelPos.xy);

	float shadow = 1;
	if (EnableShadow)
		shadow = CalcShadow(input.worldPos, normalize(input.normal));

	if (InstanceData[input.id].MaterialType == kMaterialMERL)
	{
		if (EnableDirectLight)
			output.directLight.rgb = CalcDirectLight(Buffers[MerlBRDF], LightDir.xyz, view, normal, tangent, binormal) * LightIlluminance.rgb * shadow;

		if (EnableEnvEmitter)
			output.indirectLight.rgba = CalcIndirectLight(Buffers[MerlBRDF], normal, view, tangent, binormal, random);
	}
	else
	{
		if (EnableDirectLight)
		{
			// Lo = ( Fd + Fs ) * (n,l) * E
			output.directLight.rgb = CalcDirectLight(normal, LightDir.xyz, view, materialData) * LightIlluminance.rgb * shadow;
		}
		if (EnableEnvEmitter)
		{
			if (SamplingType == kSamplingTypeIS || SamplingType == kSamplingTypeFIS)
			{
				output.indirectLight.rgba = CalcIndirectLight(normal, view, materialData, random);
			}
			else
			{
				output.directLight.rgb += ApproximatedIndirectLight(normal, view, materialData, random).rgb;
				output.indirectLight.rgb = 0;
				output.indirectLight.a = 0.0;
			}
		}
	}

	output.directLight.rgb *= ao;
	output.indirectLight.rgb *= ao;

	return output;
}