#include "Precompiled.h"
#include "EnvEmitter.h"

using namespace DirectX;


static const uint32_t kCubemapResolution = 256;


struct MipGenConstBuffer
{
	uint32_t sourceTexIdx;
	uint32_t destTexIdx;
	uint32_t mipIdx;
};


static XMMATRIX BuildCubemapMatrix(uint32_t faceIdx)
{
	const XMVECTORF32 camPos = {0.0f, 0.0f, 0.0f, 0.0f};

	const XMVECTORF32 kDir[] = {
	    {1.0f, 0.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f},
	    {0.0f, -1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f},  {0.0f, 0.0f, -1.0f, 0.0f},
	};

	const XMVECTORF32 kUp[] = {
	    {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 0.0f},
	    {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f},
	};

	return XMMatrixLookAtLH(camPos, kDir[faceIdx], kUp[faceIdx]);
}


bool EnvEmitter::Init(Device* device)
{
	PIXScopedEvent(0, "EnvEmitter::Init");
	m_device = device;
	ReloadShaders();
	if (!m_sphereModel.Load(m_device, "models\\sphere.obj", false, TextureMapping()))
		return false;

	for (int i = 0; i < 6; i++)
	{
		XMMATRIX view = BuildCubemapMatrix(i);
		XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PI / 2.0f, 1.0f, 1.0f, 5000.0f);
		m_constBufferData.BakeViewProj[i] = XMMatrixMultiply(view, proj);
	}

	// cube map
	const DXGI_FORMAT kCubemapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	m_cubemap.Init(m_device, kCubemapFormat, kCubemapResolution, kCubemapResolution, L"CubemapTexture",
	               kRenderTargetCubemap | kRenderTargetWithMips | kRenderTargetAllowRTV_DSV | kRenderTargetAllowUAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = kCubemapFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = m_cubemap.m_mipLevels;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = 6;
	srvDesc.Texture2DArray.PlaneSlice = 0;
	srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
	m_cubeMap2DArraySRV = m_device->CreateSRV(m_cubemap.texture, &srvDesc);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.Texture2DArray.ArraySize = 6;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.PlaneSlice = 0;
	m_cubemapUAVs.resize(m_cubemap.m_mipLevels);
	for (size_t i = 0; i < m_cubemapUAVs.size(); i++)
	{
		uavDesc.Texture2DArray.MipSlice = (uint32_t)i;
		m_cubemapUAVs[i] = m_device->CreateUAV(m_cubemap.texture, &uavDesc);
	}

	// dummy texture
	D3D12_RESOURCE_DESC texDesc;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.DepthOrArraySize = 6;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8_UNORM;
	texDesc.SampleDesc = {1, 0};
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_CLEAR_VALUE clearValue;
	clearValue.Format = texDesc.Format;
	memset(clearValue.Color, 0, sizeof(clearValue.Color));

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT hr = device->GetDevice()->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COMMON, nullptr,
	                                                          IID_PPV_ARGS(&m_dummyTexture));
	if (FAILED(hr))
		return false;

	m_dummyTexture->SetName(L"DummyTexture");
	m_dummyCubeSRV = m_device->CreateSRV(m_dummyTexture, nullptr);

	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	m_dummy2DSRV = m_device->CreateSRV(m_dummyTexture, &srvDesc);

	uint8_t zero[D3D12_TEXTURE_DATA_PITCH_ALIGNMENT] = {};
	m_device->BeginTransfer();
	for (uint32_t i = 0; i < 6; i++)
		m_device->UploadTextureSubresource(m_dummyTexture, 0, i, &zero, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	m_device->EndTransfer();

	return true;
}


void EnvEmitter::Shutdown()
{
	m_device->DeletePSO(m_depthPSO);
	m_device->DeletePSO(m_lightPSO);
	m_device->DeletePSO(m_cubemapBakePSO);
	m_device->DeletePSO(m_mipsGenPSO);

	m_device->DestroySRV(m_cubeMap2DArraySRV);
	for (UAVHandle& uavHandle : m_cubemapUAVs)
		m_device->DestroyUAV(uavHandle);
	m_cubemap.Release(m_device);

	m_sphereModel.Release(m_device);

	m_device->DestroySRV(m_dummy2DSRV);
	m_device->DestroySRV(m_dummyCubeSRV);
	SafeRelease(m_dummyTexture);

	m_device->DestroySRV(m_textureSRV);
	SafeRelease(m_texture);
}


void EnvEmitter::UpdateAndBindConstBuffer(ID3D12GraphicsCommandList* cmdList)
{
	m_constBufferData.CubeTextureIdx = m_constBufferData.UseCubeTexture ? m_textureSRV.idx : m_dummyCubeSRV.idx;
	m_constBufferData.TextureIdx = m_constBufferData.UseCubeTexture ? m_dummy2DSRV.idx : m_textureSRV.idx;

	D3D12_GPU_VIRTUAL_ADDRESS gpuCbAddr = m_device->UpdateConstantBuffer(&m_constBufferData, sizeof(m_constBufferData));
	cmdList->SetGraphicsRootConstantBufferView(1, gpuCbAddr);
}


void EnvEmitter::BakeCubemap(ID3D12GraphicsCommandList* cmdList)
{
	PIXScopedEvent(cmdList, 0, "BakeCubemap");
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	if (!m_cubemapBakePSO)
	{
		if (m_cubemap.TransitionTo(finalState, barriers))
			cmdList->ResourceBarrier(barriers.size(), barriers.data());
		return;
	}

	// 0 mip to rtv state
	for (uint32_t slice = 0; slice < 6; slice++)
		m_cubemap.TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET, barriers, ComputeSubresourceIndex(0, slice, m_cubemap.m_mipLevels));
	cmdList->ResourceBarrier(barriers.size(), barriers.data());
	barriers.clear();

	cmdList->OMSetRenderTargets(1, &m_cubemap.rtv, false, nullptr);

	SetViewportAndScissorRect(cmdList, kCubemapResolution, kCubemapResolution);
	float black[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	cmdList->ClearRenderTargetView(m_cubemap.rtv, black, 0, nullptr);

	cmdList->SetPipelineState(m_cubemapBakePSO);
	Draw(cmdList, 6);

	// generate mips
	if (!m_mipsGenPSO)
		return;

	PIXScopedEvent(cmdList, 0, "MipsGen");
	// 0 mip to srv state
	for (uint32_t slice = 0; slice < 6; slice++)
		m_cubemap.TransitionTo(finalState, barriers, ComputeSubresourceIndex(0, slice, m_cubemap.m_mipLevels));
	cmdList->ResourceBarrier(barriers.size(), barriers.data());
	barriers.clear();

	cmdList->SetPipelineState(m_mipsGenPSO);
	for (uint32_t mip = 1; mip < m_cubemap.m_mipLevels; mip++)
	{
		// current mip to uav state
		for (uint32_t slice = 0; slice < 6; slice++)
			m_cubemap.TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, barriers, ComputeSubresourceIndex(mip, slice, m_cubemap.m_mipLevels));
		cmdList->ResourceBarrier(barriers.size(), barriers.data());
		barriers.clear();

		MipGenConstBuffer constBuf;
		constBuf.sourceTexIdx = m_cubeMap2DArraySRV.idx;
		constBuf.destTexIdx = m_cubemapUAVs[mip].idx;
		constBuf.mipIdx = mip;
		D3D12_GPU_VIRTUAL_ADDRESS gpuCbAddr = m_device->UpdateConstantBuffer(&constBuf, sizeof(constBuf));

		cmdList->SetComputeRootConstantBufferView(1, gpuCbAddr);
		uint32_t groupsNum = (CalcMipSize(kCubemapResolution, mip) + 31) / 32;
		cmdList->Dispatch(groupsNum, groupsNum, 6);

		// current mip to srv state
		for (uint32_t slice = 0; slice < 6; slice++)
			m_cubemap.TransitionTo(finalState, barriers, ComputeSubresourceIndex(mip, slice, m_cubemap.m_mipLevels));
		cmdList->ResourceBarrier(barriers.size(), barriers.data());
		barriers.clear();
	}
}


void EnvEmitter::RenderDepthPass(ID3D12GraphicsCommandList* cmdList)
{
	if (!m_depthPSO)
		return;
	cmdList->SetPipelineState(m_depthPSO);
	Draw(cmdList, 1);
}


void EnvEmitter::RenderLightPass(ID3D12GraphicsCommandList* cmdList)
{
	if (!m_lightPSO)
		return;
	cmdList->SetPipelineState(m_lightPSO);
	Draw(cmdList, 1);
}


void EnvEmitter::Draw(ID3D12GraphicsCommandList* cmdList, uint32_t numInstances)
{
	UpdateAndBindConstBuffer(cmdList);

	cmdList->IASetVertexBuffers(0, 1, &m_sphereModel.vbv);
	cmdList->IASetIndexBuffer(&m_sphereModel.ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (Mesh& mesh : m_sphereModel.meshes)
		cmdList->DrawIndexedInstanced(mesh.indexCount, numInstances, mesh.firstIndex, mesh.firstVertex, 0);
}


bool EnvEmitter::ReloadShaders()
{
	m_device->DeletePSO(m_depthPSO);
	m_device->DeletePSO(m_lightPSO);
	m_device->DeletePSO(m_cubemapBakePSO);
	m_device->DeletePSO(m_mipsGenPSO);

	GfxPipelineStateDesc psoDesc;
	psoDesc.name = L"EnvEmitterDepth";
	psoDesc.InputLayout = {kMeshElementsDesc, _countof(kMeshElementsDesc)};
	psoDesc.vs = L"env_emitter.hlsl";
	psoDesc.RasterizerState = GetDefaultRasterizerDesc();
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	psoDesc.BlendState = GetDefaultBlendDesc();
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
	psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = 0;
	psoDesc.DepthStencilState = GetDefaultDepthStencilDesc();
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	m_depthPSO = m_device->CreatePSO(psoDesc);

	psoDesc.name = L"EnvEmitterLight";
	psoDesc.ps = L"env_emitter.hlsl";
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	m_lightPSO = m_device->CreatePSO(psoDesc);

	psoDesc.name = L"EnvEmitterCubemapBake";
	psoDesc.gs = L"env_emitter.hlsl";
	psoDesc.defines.push_back(L"CUBEMAP_BAKE");
	psoDesc.DepthStencilState = GetDefaultDepthStencilDesc();
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	m_cubemapBakePSO = m_device->CreatePSO(psoDesc);

	ComputePipelineStateDesc csDesc;
	csDesc.name = L"MipsGenerator";
	csDesc.cs = L"mips_generator.hlsl";
	m_mipsGenPSO = m_device->CreatePSO(csDesc);

	return S_OK;
}


void EnvEmitter::SetTexture(const char* filename)
{
	m_device->DestroySRV(m_textureSRV);
	m_device->DestroyResource(m_texture);

	m_textureFileName = filename;
	FilePathW filenameW = ConvertPath(filename);

	FilePathW filepath = L"data";
	filepath /= L"HDRs";
	filepath /= filenameW;

	TexMetadata metadata;
	ScratchImage scratchImage;
	if (!::LoadTexture(filepath, &metadata, scratchImage))
		return;

	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = (UINT)metadata.width;
	desc.Height = (UINT)metadata.height;
	desc.DepthOrArraySize = (UINT16)metadata.arraySize;
	desc.MipLevels = (UINT16)metadata.mipLevels;
	desc.Format = metadata.format;
	desc.SampleDesc = {1, 0};
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT hr =
	    m_device->GetDevice()->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_texture));
	if (FAILED(hr))
		return;
	m_texture->SetName(filenameW.c_str());

	m_device->BeginTransfer();
	for (uint32_t arrayIdx = 0; arrayIdx < desc.DepthOrArraySize; arrayIdx++)
	{
		for (uint32_t mip = 0; mip < desc.MipLevels; mip++)
		{
			const Image* image = scratchImage.GetImage(mip, arrayIdx, 0);
			m_device->UploadTextureSubresource(m_texture, mip, arrayIdx, image->pixels, image->rowPitch);
		}
	}
	m_device->EndTransfer();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = desc.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if (metadata.IsCubemap())
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = desc.MipLevels;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	m_textureSRV = m_device->CreateSRV(m_texture, &srvDesc);
	m_constBufferData.UseCubeTexture = metadata.IsCubemap() ? 1 : 0;
}