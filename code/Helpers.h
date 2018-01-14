#pragma once

struct Float4
{
	float x;
	float y;
	float z;
	float w;
};

inline Float4 ToFloat4(const XMVECTOR& v)
{
	Float4 ret;
	memcpy(&ret, &v, sizeof(v));
	return ret;
}

float ToRad(float deg);
float ToDeg(float rad);
DirectX::XMVECTOR PackedSRGBToSRGB(uint32_t color);
DirectX::XMVECTOR PackedSRGBToLinear(uint32_t color);
uint32_t LinearToPackedSRGB(const DirectX::XMVECTOR& v);
bool LoadTexture(const FilePathW& filepath, DirectX::TexMetadata* metadata, DirectX::ScratchImage& image);
