#include "Precompiled.h"
#include <algorithm>

using namespace DirectX;


float ToRad(float deg)
{
	return deg * XM_PI / 180.0f;
}


float ToDeg(float rad)
{
	return rad * 180.0f / XM_PI;
}


DirectX::XMVECTOR PackedSRGBToSRGB(uint32_t color)
{
	float r = (float)(color & 255) / 255.0f;
	float g = (float)((color >> 8) & 255) / 255.0f;
	float b = (float)((color >> 16) & 255) / 255.0f;
	return XMVectorSet(r, g, b, 0.0f);
}


XMVECTOR PackedSRGBToLinear(uint32_t color)
{
	float r = (float)(color & 255) / 255.0f;
	float g = (float)((color >> 8) & 255) / 255.0f;
	float b = (float)((color >> 16) & 255) / 255.0f;
	return XMColorSRGBToRGB(XMVectorSet(r, g, b, 0.0f));
}


uint32_t LinearToPackedSRGB(const XMVECTOR& v)
{
	XMVECTOR srgb = XMColorRGBToSRGB(v);
	DWORD ret = 0;
	ret |= (DWORD)(srgb.m128_f32[0] * 255.0f);
	ret |= (DWORD)(srgb.m128_f32[1] * 255.0f) << 8;
	ret |= (DWORD)(srgb.m128_f32[2] * 255.0f) << 16;
	return ret;
}


struct WICInitializer
{
	WICInitializer()
	{
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	}

	~WICInitializer()
	{
		CoUninitialize();
	}
};


bool LoadTexture(const FilePathW& filepath, TexMetadata* metadata, ScratchImage& image)
{
	static WICInitializer wicInitializer;

	File file(filepath.c_str(), File::kOpenRead);
	if (!file.IsOpened())
		return false;

	std::unique_ptr<uint8_t[]> data(new uint8_t[file.GetSize()]);
	file.Read(data.get(), file.GetSize());

	FilePathW ext = filepath.GetExtension();
	if (ext == L".dds")
		return LoadFromDDSMemory(data.get(), file.GetSize(), DDS_FLAGS_NONE, metadata, image) == S_OK;
	else if (ext == L".hdr")
		return LoadFromHDRMemory(data.get(), file.GetSize(), metadata, image) == S_OK;
	else if (ext == L".tga")
		return LoadFromTGAMemory(data.get(), file.GetSize(), metadata, image) == S_OK;
	else
		return LoadFromWICMemory(data.get(), file.GetSize(), WIC_FLAGS_NONE, metadata, image) == S_OK;
}