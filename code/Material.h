#pragma once
#include <string>

class Material
{
public:
	enum ETextures
	{
		kBaseColor = 0,
		kNormal,
		kRoughness,
		kMetalness,
		kAO,
		kTexturesCount
	};

	bool Load(Device* device, const char* materialName);
	bool Load(Device* device, const wchar_t* baseColorPath, const wchar_t* normalPath, const wchar_t* roughnessPath, const wchar_t* metalnessPath, const wchar_t* ao);
	void Release(Device* device);

	SRVHandle GetTexture(ETextures texture);

private:
	ID3D12Heap* m_heap = nullptr;
	ID3D12Resource* m_resources[kTexturesCount] = {};
	SRVHandle m_srvs[kTexturesCount];
};


inline SRVHandle Material::GetTexture(ETextures texture)
{
	return m_srvs[texture];
}


bool ImportMaterial(const wchar_t* dirPath);