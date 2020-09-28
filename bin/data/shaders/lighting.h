#include "hammersley.h"
#include "merl.h"

// http://graphicrants.blogspot.ru/2013/08/specular-brdf-reference.html

static const float DIELECTRIC_SPEC = 0.04f;
static const float PI = 3.14159265359f;
static const float INV_PI = 0.31830988618f;

static const uint kMaterialSimple = 0;
static const uint kMaterialSmoothDiffuse = 1;
static const uint kMaterialRoughDiffuse = 2;
static const uint kMaterialSmoothConductor = 3;
static const uint kMaterialRoughConductor = 4;
static const uint kMaterialRoughPlastic = 5;
static const uint kMaterialTexture = 6;
static const uint kMaterialMERL = 7;

static const uint kSamplingTypeIS = 0;
static const uint kSamplingTypeFIS = 1;
static const uint kSamplingTypeSplitSum = 2;
static const uint kSamplingTypeSplitSumNV = 3;
static const uint kSamplingTypeBakedSplitSumNV = 4;


struct MaterialData
{
	uint type;
	float3 albedo;
	float3 F0;
	float roughness;
};


float PerceptualRoughnessToRoughness(float perceptualRoughness)
{
	return perceptualRoughness * perceptualRoughness;
}


void CalcAlbedoAndF0(float3 baseColor, float metalness, float reflectance, out float3 albedo, out float3 F0)
{
	float dielectricSpec = DIELECTRIC_SPEC * reflectance;
	float oneMinusReflectivity = (1.0f - dielectricSpec) * (1.0f - metalness);
	F0 = lerp(dielectricSpec, baseColor, metalness);
	albedo = baseColor * oneMinusReflectivity;
}


MaterialData InitMaterialData(uint type, float metalness, float perceptualRoughness, float reflectance, float3 baseColor)
{
	MaterialData data = (MaterialData)0;
	data.type = type;
	data.roughness = PerceptualRoughnessToRoughness(perceptualRoughness);
	switch (type)
	{
		case kMaterialSimple:
		case kMaterialTexture:
			CalcAlbedoAndF0(baseColor, metalness, reflectance, data.albedo, data.F0);
			break;
		case kMaterialSmoothDiffuse:
		case kMaterialRoughDiffuse:
			data.albedo = baseColor;
			data.F0 = 0.0f;
			if (type == kMaterialSmoothDiffuse)
				data.roughness = 0.0f;
			break;
		case kMaterialSmoothConductor:
		case kMaterialRoughConductor:
			data.albedo = 0.0f;
			data.F0 = baseColor;
			if (type == kMaterialSmoothConductor)
				data.roughness = 0.0f;
			break;
	}
	return data;
}


// Ref: http://jcgt.org/published/0003/02/03/paper.pdf
// Vis = G / ( 4 * NoL * NoV )
float Vis_SmithJointGGX(float NoL, float NoV, float roughness)
{
	// Approximated version
	float a = roughness;
	float lambdaV = NoL * (NoV * (1.0f - a) + a);
	float lambdaL = NoV * (NoL * (1.0f - a) + a);
	return 0.5f * rcp(lambdaV + lambdaL);
}


float D_GGX(float NoH, float roughness)
{
	float a2 = roughness * roughness;
	float d = (NoH * a2 - NoH) * NoH + 1.0f;  // 2 mad
	return INV_PI * a2 / (d * d);
}


float3 F_Schlick(float3 F0, float VoH)
{
	float Fc = pow(1 - VoH, 5);
	return (1 - Fc) * F0 + Fc;
}


float3 DiffuseBRDF(float3 N, float3 L, float3 V, MaterialData materialData)
{
	if (!EnableDiffuseBRDF)
		return 0.0f;

	if (materialData.type == kMaterialSimple || materialData.type == kMaterialTexture || materialData.type == kMaterialSmoothDiffuse)
	{
		return materialData.albedo / PI;
	}
	else if (materialData.type == kMaterialRoughDiffuse)
	{
		return 0.0f;  // TODO
	}

	return 0.0f;
}


float3 SpecularBRDF(float3 N, float3 L, float3 V, MaterialData materialData)
{
	if (!EnableSpecularBRDF)
		return 0.0f;

	if (materialData.type == kMaterialSimple || materialData.type == kMaterialTexture || materialData.type == kMaterialSmoothConductor ||
	    materialData.type == kMaterialRoughConductor)
	{
		float3 H = normalize(V + L);
		float NoV = abs(dot(N, V)) + 1e-5f;
		float NoL = saturate(dot(N, L));
		float NoH = saturate(dot(N, H));
		float VoH = saturate(dot(V, H));

		// Micro-facet specular
		// Vis = G / ( 4 * NoL * NoV )
		float Vis = Vis_SmithJointGGX(NoL, NoV, materialData.roughness);
		float D = D_GGX(NoH, materialData.roughness);
		float3 F = F_Schlick(materialData.F0, VoH);
		// D * F * G / ( 4 * NoL * NoV ) = Vis * D * F
		return Vis * D * F;
	}

	return 0.0f;
}


float3 CalcDirectLight(float3 N, float3 L, float3 V, MaterialData materialData)
{
	float NoL = saturate(dot(N, L));
	materialData.roughness = max(materialData.roughness, 1e-04f);
	float3 diffuse = DiffuseBRDF(N, L, V, materialData);
	float3 specular = SpecularBRDF(N, L, V, materialData);
	return (diffuse + specular) * NoL;
}


float3 CalcDirectLight(Buffer<float> merlBrdf, float3 L, float3 V, float3 normal, float3 tangent, float3 binormal)
{
	return EvaluteMerlBRDF(merlBrdf, L, V, normal, tangent, binormal);
}


float3 ImportanceSampleGGX(float2 E, float Roughness, float3 N)
{
	float m2 = Roughness * Roughness;

	float Phi = 2 * PI * E.x;
	float CosTheta = sqrt((1 - E.y) / (1 + (m2 - 1) * E.y));
	float SinTheta = sqrt(1 - CosTheta * CosTheta);

	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;

	float3 UpVector = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 TangentX = normalize(cross(UpVector, N));
	float3 TangentY = cross(N, TangentX);
	// Tangent to world space
	return TangentX * H.x + TangentY * H.y + N * H.z;
}


float3 ImportanceSampleDiffuse(float2 Xi, float3 N)
{
	float CosTheta = 1.0f - Xi.y;
	float SinTheta = sqrt(1.0 - CosTheta * CosTheta);
	float Phi = 2.0f * PI * Xi.x;

	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;

	float3 UpVector = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 TangentX = normalize(cross(UpVector, N));
	float3 TangentY = cross(N, TangentX);

	return TangentX * H.x + TangentY * H.y + N * H.z;
}


float4 CalcIndirectLight(float3 N, float3 V, MaterialData materialData, uint2 random)
{
	if (SamplesProcessed >= TotalSamples)
		return 0;

	uint cubeWidth, cubeHeight;
	TexturesCube[EnvironmentMap].GetDimensions(cubeWidth, cubeHeight);

	float3 specular = 0;
	float3 diffuse = 0;
	for (uint i = 0; i < SamplesInStep; i++)
	{
		float2 Xi = Hammersley_v1(i + SamplesProcessed, TotalSamples, random);
		if (EnableSpecularBRDF && (materialData.type == kMaterialSimple || materialData.type == kMaterialTexture ||
		                           materialData.type == kMaterialSmoothConductor || materialData.type == kMaterialRoughConductor))
		{
			float3 H = ImportanceSampleGGX(Xi, materialData.roughness, N);
			float3 L = 2 * dot(V, H) * H - V;
			float NoV = abs(dot(N, V)) + 1e-5f;
			float NoL = saturate(dot(N, L));
			float NoH = saturate(dot(N, H));
			float VoH = saturate(dot(V, H));
			if (NoL > 0)
			{
				float pdf = D_GGX(NoH, materialData.roughness) * NoH / (4 * VoH);
				float lod = 0;
				if (SamplingType == kSamplingTypeFIS)
				{
					float solidAngleTexel = 4 * PI / (6 * cubeWidth * cubeWidth);
					float solidAngleSample = 1.0 / (TotalSamples * pdf);
					lod = materialData.roughness == 0 ? 0 : max(0.5 * log2(solidAngleSample / solidAngleTexel), 0.0f);
				}
				float3 sampleColor = TexturesCube[EnvironmentMap].SampleLevel(LinearWrapSampler, L, lod).rgb;

				// specular += sampleColor * SpecularBRDF(N, L, V, materialData) * NoL / pdf;
				float Vis = Vis_SmithJointGGX(NoL, NoV, materialData.roughness);
				float3 F = F_Schlick(materialData.F0, VoH);
				specular += sampleColor * F * (NoL * Vis * (4 * VoH / NoH));
			}
		}
		if (EnableDiffuseBRDF)
		{
			float3 L = ImportanceSampleDiffuse(Xi, N);
			// L = normalize( L );
			float NoL = saturate(dot(N, L));
			if (NoL > 0)
			{
				float pdf = NoL * INV_PI;
				float lod = 0;
				if (SamplingType == kSamplingTypeFIS)
				{
					float solidAngleTexel = 4 * PI / (6 * cubeWidth * cubeWidth);
					float solidAngleSample = 1.0 / (TotalSamples * pdf);
					lod = 0.5 * log2((float)(solidAngleSample / solidAngleTexel));
				}
				float3 sampleColor = TexturesCube[EnvironmentMap].SampleLevel(LinearWrapSampler, L, lod).rgb;

				diffuse += sampleColor * DiffuseBRDF(N, L, V, materialData) * NoL / pdf;
			}
		}
	}

	return float4(diffuse + specular, SamplesInStep);
}


float4 CalcIndirectLight(Buffer<float> merlBrdf, float3 N, float3 V, float3 tangent, float3 binormal, uint2 random)
{
	if (SamplesProcessed >= TotalSamples)
		return 0;

	uint cubeWidth, cubeHeight;
	TexturesCube[EnvironmentMap].GetDimensions(cubeWidth, cubeHeight);

	float3 lighting = 0;
	for (uint i = 0; i < SamplesInStep; i++)
	{
		float2 Xi = Hammersley_v1(i + SamplesProcessed, TotalSamples, random);

		float3 L = ImportanceSampleDiffuse(Xi, N);
		L = normalize(L);
		float NoL = saturate(dot(N, L));
		if (NoL > 0)
		{
			float3 SampleColor = TexturesCube[EnvironmentMap].SampleLevel(LinearWrapSampler, L, 0).rgb;
			SampleColor *= EvaluteMerlBRDF(merlBrdf, L, V, N, tangent, binormal);
			lighting += SampleColor * sqrt(1.0 - NoL * NoL);
		}
	}

	return float4(lighting * PI * PI, SamplesInStep);
}


float3 PrefilterSpecularEnvMap(float roughness, float3 N, float3 V, uint2 random)
{
	uint cubeWidth, cubeHeight;
	TexturesCube[EnvironmentMap].GetDimensions(cubeWidth, cubeHeight);

	float weight = 0.0f;
	float3 accum = 0.0f;
	for (uint i = 0; i < TotalSamples; i++)
	{
		float2 Xi = Hammersley_v1(i, TotalSamples, random);
		float3 H = ImportanceSampleGGX(Xi, roughness, N);
		float3 L = 2 * dot(V, H) * H - V;
		float NoV = abs(dot(N, V)) + 1e-5f;
		float NoL = saturate(dot(N, L));
		float NoH = saturate(dot(N, H));
		float VoH = saturate(dot(V, H));
		if (NoL > 0)
		{
			float pdf = D_GGX(NoH, roughness) * NoH / (4 * VoH);

			float solidAngleTexel = 4 * PI / (6 * cubeWidth * cubeWidth);
			float solidAngleSample = 1.0 / (TotalSamples * pdf);
			float lod = roughness == 0 ? 0 : max(0.5 * log2(solidAngleSample / solidAngleTexel), 0.0f);

			accum += TexturesCube[EnvironmentMap].SampleLevel(LinearWrapSampler, L, lod).rgb * NoL;
			weight += NoL;
		}
	}

	// weight = TotalSamples;
	return accum / weight;
}


float3 PrefilterDiffuseEnvMap(float3 N, uint2 random)
{
	uint cubeWidth, cubeHeight;
	TexturesCube[EnvironmentMap].GetDimensions(cubeWidth, cubeHeight);

	float weight = 0.0f;
	float3 accum = 0.0f;
	for (uint i = 0; i < TotalSamples; i++)
	{
		float2 Xi = Hammersley_v1(i, TotalSamples, random);
		float3 L = ImportanceSampleDiffuse(Xi, N);
		float NoL = saturate(dot(N, L));
		if (NoL > 0)
		{
			// Compute Lod using inverse solid angle and pdf.
			// From Chapter 20.4 Mipmap filtered samples in GPU Gems 3.
			// http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html
			float pdf = NoL * INV_PI;

			float solidAngleTexel = 4 * PI / (6 * cubeWidth * cubeWidth);
			float solidAngleSample = 1.0 / (TotalSamples * pdf);
			float lod = 0.5 * log2((float)(solidAngleSample / solidAngleTexel));

			accum += TexturesCube[EnvironmentMap].SampleLevel(LinearWrapSampler, L, lod).rgb;
		}
	}

	weight = TotalSamples;
	return accum / weight;
}


float2 GenerateBRDFLut(float roughness, float NoV, uint2 random)
{
	// Normal always points along z-axis for the 2D lookup
	const float3 N = float3(0.0, 0.0, 1.0);
	float3 V = float3(sqrt(1.0 - NoV * NoV), 0.0, NoV);

	float2 lut = float2(0.0, 0.0);
	for (uint i = 0; i < TotalSamples; i++)
	{
		float2 Xi = Hammersley_v1(i, TotalSamples, random);
		float3 H = ImportanceSampleGGX(Xi, roughness, N);
		float3 L = 2 * dot(V, H) * H - V;
		float NoL = saturate(dot(N, L));
		float NoH = saturate(dot(N, H));
		float VoH = saturate(dot(V, H));
		if (NoL > 0)
		{
			float Vis = Vis_SmithJointGGX(NoL, NoV, roughness) * NoL * (4 * VoH / NoH);
			float Fc = pow(1.0 - VoH, 5.0);

			lut.x += Vis * (1.0 - Fc);
			lut.y += Vis * Fc;
		}
	}
	return lut / float(TotalSamples);
}


float3 GetSpecularDominantDir(float3 N, float3 R, float roughness)
{
	float smoothness = saturate(1 - roughness);
	float lerpFactor = smoothness * (sqrt(smoothness) + roughness);
	// The result is not normalized as we fetch in a cubemap
	return lerp(N, R, lerpFactor);
}


float3 ApproximatedIndirectLight(float3 N, float3 V, MaterialData materialData, uint2 random)
{
	float NoV = abs(dot(N, V));

	float3 R = 2 * dot(V, N) * N - V;

	float3 L = 0;
	float2 lut = 0;

	if (SamplingType == kSamplingTypeSplitSum)
	{
		L = PrefilterSpecularEnvMap(materialData.roughness, N, V, random);
		lut = GenerateBRDFLut(materialData.roughness, NoV, random);
	}
	else if (SamplingType == kSamplingTypeSplitSumNV)
	{
		L = PrefilterSpecularEnvMap(materialData.roughness, R, R, random);
		lut = GenerateBRDFLut(materialData.roughness, NoV, random);
	}
	else if (SamplingType == kSamplingTypeBakedSplitSumNV)
	{
		float width, height, numberOfLevels;
		TexturesCube[PrefilteredSpecularEnvMap].GetDimensions(0, width, height, numberOfLevels);
		float mip = sqrt(materialData.roughness) * numberOfLevels;
		R = GetSpecularDominantDir(N, R, materialData.roughness);
		L = TexturesCube[PrefilteredSpecularEnvMap].SampleLevel(LinearWrapSampler, R, mip).rgb;
		lut = Textures2D[BRDFLut].Sample(LinearClampSampler, float2(materialData.roughness, NoV)).xy;
	}

	float3 ret = 0;
	if (EnableSpecularBRDF)
		ret += L * (materialData.F0 * lut.x + lut.y);

	L = 0;
	if (SamplingType == kSamplingTypeSplitSum || SamplingType == kSamplingTypeSplitSumNV)
	{
		L = PrefilterDiffuseEnvMap(N, random);
	}
	else if (SamplingType == kSamplingTypeBakedSplitSumNV)
	{
		L = TexturesCube[PrefilteredDiffuseEnvMap].Sample(LinearWrapSampler, N).rgb;
	}

	if (EnableDiffuseBRDF)
		ret += materialData.albedo * L;

	return ret;
}


float CalcShadow(float3 worldPos, float3 normal)
{
	float4 pos = float4(worldPos, 1.0f);
	float3 uv = mul(pos, ShadowMatrix).xyz;

	float2 shadowMapSize;
	float numSlices;
	Textures2D[ShadowMap].GetDimensions(0, shadowMapSize.x, shadowMapSize.y, numSlices);
	float texelSize = 2.0f / shadowMapSize.x;
	float nmlOffsetScale = saturate(1.0f - dot(LightDir.xyz, normal));
	float offsetScale = 1.0f;
	float3 normalOffset = texelSize * offsetScale * nmlOffsetScale * normal;

	pos = float4(worldPos + normalOffset, 1.0f);
	uv.xyz = mul(pos, ShadowMatrix).xyz;
	uv.z -= texelSize.x;

	return Textures2D[ShadowMap].SampleCmpLevelZero(CmpLinearSampler, uv.xy, uv.z);
}