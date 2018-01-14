#pragma once
#include <assimp/material.h>
#include "Material.h"

enum TextureType
{
	kTextureDiffuse = 0,
	kTextureNormal,
	kTextureRoughness,
	kTextureMetalness,

	kTextureTypeCount
};


struct TextureMapping
{
	aiTextureType mapping[kTextureTypeCount];

	TextureMapping()
	{
		memset(mapping, 0, kTextureTypeCount * sizeof(aiTextureType));
	}

	const aiTextureType& operator[](const TextureType t) const
	{
		return mapping[t];
	}

	aiTextureType& operator[](const TextureType t)
	{
		return mapping[t];
	}
};


struct MeshVertex
{
	float pos[3];
	float normal[3];
	float uv[2];
	float tan[3];
	float binormal[3];
};


static const D3D12_INPUT_ELEMENT_DESC kMeshElementsDesc[] = {
    // clang-format off
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(MeshVertex, pos),      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(MeshVertex, normal),   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXTURE",  0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(MeshVertex, uv),       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(MeshVertex, tan),      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(MeshVertex, binormal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    // clang-format on
};


struct Mesh
{
	uint32_t firstVertex = 0;
	uint32_t firstIndex = 0;
	uint32_t indexCount = 0;
	uint32_t materialIdx = 0;
};


struct Model
{
	bool Load(Device* device, const char* filename, bool loadMaterials, const TextureMapping& textureMapping);
	void Release(Device* device);

	FilePath name;
	std::vector<Material> materials;
	ID3D12Resource* buffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	D3D12_INDEX_BUFFER_VIEW ibv;
	std::vector<Mesh> meshes;
};