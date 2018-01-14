#include "Precompiled.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <vector>
#include "Model.h"
#include "Material.h"


bool Model::Load(Device* device, const char* filename, bool loadMaterials, const TextureMapping& textureMapping)
{
	PIXScopedEvent(0, "Model::Load '%s'", filename);
	name = filename;
	Assimp::Importer Importer;

	FilePath filepath = "data";
	filepath /= filename;

	int flags = aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded;
	const aiScene* aScene = Importer.ReadFile(filepath.c_str(), flags);
	if (!aScene)
		return false;

	if (loadMaterials)
	{
		FilePath folder = filepath.GetParentPath();
		materials.resize(aScene->mNumMaterials);
		for (uint32_t i = 0; i < aScene->mNumMaterials; i++)
		{
			aiString aistring;
			aScene->mMaterials[i]->Get(AI_MATKEY_NAME, aistring);

			const uint32_t kMaxFileNameLen = 256;
			wchar_t baseColorPath[kMaxFileNameLen] = {};
			wchar_t normalPath[kMaxFileNameLen] = {};
			wchar_t roughnessPath[kMaxFileNameLen] = {};
			wchar_t metalnessPath[kMaxFileNameLen] = {};
			wchar_t ao[kMaxFileNameLen] = {};

			if (aScene->mMaterials[i]->GetTexture(textureMapping[kTextureDiffuse], 0, &aistring) == aiReturn_SUCCESS)
				wsprintf(baseColorPath, L"%S\\%S", folder.c_str(), aistring.C_Str());

			if (aScene->mMaterials[i]->GetTexture(textureMapping[kTextureNormal], 0, &aistring) == aiReturn_SUCCESS)
				wsprintf(normalPath, L"%S\\%S", folder.c_str(), aistring.C_Str());

			if (aScene->mMaterials[i]->GetTexture(textureMapping[kTextureMetalness], 0, &aistring) == aiReturn_SUCCESS)
				wsprintf(metalnessPath, L"%S\\%S", folder.c_str(), aistring.C_Str());

			if (aScene->mMaterials[i]->GetTexture(textureMapping[kTextureRoughness], 0, &aistring) == aiReturn_SUCCESS)
				wsprintf(roughnessPath, L"%S\\%S", folder.c_str(), aistring.C_Str());

			materials[i].Load(device, baseColorPath, normalPath, roughnessPath, metalnessPath, ao);
		}
	}

	uint32_t verticesNum = 0;
	uint32_t indicesNum = 0;
	for (uint32_t i = 0; i < aScene->mNumMeshes; i++)
	{
		verticesNum += aScene->mMeshes[i]->mNumVertices;
		indicesNum += aScene->mMeshes[i]->mNumFaces * 3;
	}

	bool use32BitIndex = verticesNum > 0xffff;
	uint32_t indexSize = use32BitIndex ? sizeof(uint32_t) : sizeof(uint16_t);
	DXGI_FORMAT indexFormat = use32BitIndex ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

	uint32_t vertexBufferSize = verticesNum * sizeof(MeshVertex);
	vertexBufferSize = Align(vertexBufferSize, 16);
	uint32_t indexBufferSize = indicesNum * indexSize;
	uint32_t bufferSize = vertexBufferSize + indexBufferSize;

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = 0;
	bufferDesc.Width = bufferSize;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc = {1, 0};
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	HRESULT hr = device->GetDevice()->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr,
	                                                          IID_PPV_ARGS(&buffer));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateCommittedResource failed: %x\n", hr);
		return false;
	}
	//buffer->SetName()

	vbv.BufferLocation = buffer->GetGPUVirtualAddress();
	vbv.SizeInBytes = vertexBufferSize;
	vbv.StrideInBytes = sizeof(MeshVertex);

	ibv.BufferLocation = buffer->GetGPUVirtualAddress() + vertexBufferSize;
	ibv.Format = indexFormat;
	ibv.SizeInBytes = indexBufferSize;

	device->BeginTransfer();
	uint8_t* uploadBuffer = device->PrepareForBufferUpload(bufferSize);
	MeshVertex* vertices = (MeshVertex*)uploadBuffer;
	uint32_t verticesCounter = 0;
	uint32_t* indices32Bit = (uint32_t*)(uploadBuffer + vertexBufferSize);
	uint16_t* indices16Bit = (uint16_t*)(uploadBuffer + vertexBufferSize);
	uint32_t indicesCounter = 0;
	meshes.resize(aScene->mNumMeshes);
	for (uint32_t i = 0; i < aScene->mNumMeshes; i++)
	{
		aiMesh* aMesh = aScene->mMeshes[i];
		for (uint32_t v = 0; v < aMesh->mNumVertices; v++)
		{
			memcpy(vertices->pos, &aMesh->mVertices[v], sizeof(vertices->pos));
			memcpy(vertices->normal, &aMesh->mNormals[v], sizeof(vertices->normal));
			memcpy(vertices->uv, &aMesh->mTextureCoords[0][v], sizeof(vertices->uv));
			memcpy(vertices->tan, &aMesh->mTangents[v], sizeof(vertices->tan));
			memcpy(vertices->binormal, &aMesh->mBitangents[v], sizeof(vertices->binormal));
			vertices++;
		}

		for (uint32_t f = 0; f < aMesh->mNumFaces; f++)
		{
			if (use32BitIndex)
			{
				*indices32Bit++ = aMesh->mFaces[f].mIndices[0];
				*indices32Bit++ = aMesh->mFaces[f].mIndices[1];
				*indices32Bit++ = aMesh->mFaces[f].mIndices[2];
			}
			else
			{
				*indices16Bit++ = aMesh->mFaces[f].mIndices[0];
				*indices16Bit++ = aMesh->mFaces[f].mIndices[1];
				*indices16Bit++ = aMesh->mFaces[f].mIndices[2];
			}
		}

		Mesh& mesh = meshes[i];
		mesh.firstVertex = verticesCounter;
		mesh.firstIndex = indicesCounter;
		mesh.indexCount = aMesh->mNumFaces * 3;
		mesh.materialIdx = aMesh->mMaterialIndex;

		verticesCounter += aMesh->mNumVertices;
		indicesCounter += aMesh->mNumFaces * 3;
	}
	device->UploadBuffer(buffer, 0);
	device->EndTransfer();

	return true;
}


void Model::Release(Device* device)
{
	for (Material& material : materials)
		material.Release(device);
	materials.clear();
	
	device->DestroyResource(buffer);
	meshes.clear();
}
