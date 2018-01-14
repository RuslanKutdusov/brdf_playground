#include "Precompiled.h"
#include "Material.h"


struct ImageWrap
{
	Material::ETextures texture;
	DirectX::TexMetadata metadata;
	DirectX::ScratchImage image;
};


bool Material::Load(Device* device_, const char* materialName)
{
	const wchar_t* extension = L"dds";
	const uint32_t kMaxFileNameLen = 256;
	wchar_t baseColor[kMaxFileNameLen] = {};
	wchar_t normal[kMaxFileNameLen] = {};
	wchar_t roughness[kMaxFileNameLen] = {};
	wchar_t metalness[kMaxFileNameLen] = {};
	wchar_t ao[kMaxFileNameLen] = {};

	wsprintf(baseColor, L"data\\materials\\%S\\basecolor.%s", materialName, extension);
	wsprintf(normal, L"data\\materials\\%S\\normal.%s", materialName, extension);
	wsprintf(roughness, L"data\\materials\\%S\\roughness.%s", materialName, extension);
	wsprintf(metalness, L"data\\materials\\%S\\metalness.%s", materialName, extension);
	wsprintf(ao, L"data\\materials\\%S\\ao.%s", materialName, extension);

	return Load(device_, baseColor, normal, roughness, metalness, ao);
}


bool Material::Load(Device* device,
                    const wchar_t* baseColorPath,
                    const wchar_t* normalPath,
                    const wchar_t* roughnessPath,
                    const wchar_t* metalnessPath,
                    const wchar_t* ao)
{
	Release(device);

	const wchar_t* paths[kTexturesCount] = {baseColorPath, normalPath, roughnessPath, metalnessPath, ao};

	HRESULT hr;
	std::vector<ImageWrap> images;
	std::vector<D3D12_RESOURCE_DESC> descs;
	for (uint32_t i = 0; i < kTexturesCount; i++)
	{
		ImageWrap image;
		image.texture = (ETextures)i;
		if (!LoadTexture(paths[i], &image.metadata, image.image))
			continue;

		D3D12_RESOURCE_DESC desc;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = 0;
		desc.Width = (UINT)image.metadata.width;
		desc.Height = (UINT)image.metadata.height;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = (UINT16)image.metadata.mipLevels;
		desc.Format = image.metadata.format;
		desc.SampleDesc = {1, 0};
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		images.push_back(std::move(image));
		descs.push_back(desc);
	}

	if (descs.empty())
		return false;

	// create heap
	D3D12_RESOURCE_ALLOCATION_INFO allocInfo = device->GetDevice()->GetResourceAllocationInfo(0, (UINT)descs.size(), descs.data());

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_HEAP_DESC heapDesc;
	heapDesc.SizeInBytes = allocInfo.SizeInBytes;
	heapDesc.Alignment = allocInfo.Alignment;
	heapDesc.Properties = heapProp;
	heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
	hr = device->GetDevice()->CreateHeap(&heapDesc, IID_PPV_ARGS(&m_heap));
	if (FAILED(hr))
		return false;

	// create resources and upload
	device->BeginTransfer();
	uint64_t heapOffset = 0;
	for (size_t i = 0; i < images.size(); i++)
	{
		allocInfo = device->GetDevice()->GetResourceAllocationInfo(0, 1, &descs[i]);
		heapOffset = Align(heapOffset, (uint32_t)allocInfo.Alignment);

		ETextures tex = images[i].texture;
		hr = device->GetDevice()->CreatePlacedResource(m_heap, heapOffset, &descs[i], D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resources[tex]));
		if (FAILED(hr))
			return false;
		m_resources[tex]->SetName(paths[tex]);
		heapOffset += allocInfo.SizeInBytes;

		m_srvs[tex] = device->CreateSRV(m_resources[tex], nullptr);

		for (uint32_t mip = 0; mip < images[i].metadata.mipLevels; mip++)
		{
			const Image* image = images[i].image.GetImage(mip, 0, 0);
			device->UploadTextureSubresource(m_resources[tex], mip, 0, image->pixels, image->rowPitch);
		}
	}
	device->EndTransfer();

	return true;
}


void Material::Release(Device* device)
{
	for (SRVHandle& srv : m_srvs)
	{
		device->DestroySRV(srv);
		srv.Invalidate();
	}
	for (ID3D12Resource*& resource : m_resources)
	{
		device->DestroyResource(resource);
		resource = nullptr;
	}
	device->DestroyResource(m_heap);
	m_heap = nullptr;
}
