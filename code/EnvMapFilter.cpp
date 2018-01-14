#include "Precompiled.h"
#include "RenderTarget.h"
#include "EnvMapFilter.h"


const uint32_t SPEC_CUBEMAP_RESOLUTION = 256;
const uint32_t DIFF_CUBEMAP_RESOLUTION = 128;
const uint32_t BRDF_LUT_SIZE = 256;
const uint32_t THREAD_GROUP_SIZE = 32;


struct PrefilterConstants
{
	uint32_t PrefilteredEnvMapUAVIdx;
	uint32_t MipIndex;
	uint32_t MipsNumber;
};


struct LutGenConstants
{
	uint32_t BRDFLutUAVIdx;
};


bool EnvMapFilter::Init(Device* device)
{
	PIXScopedEvent(0, "EnvMapFilter::Init");
	m_device = device;

	ReloadShaders();

	// prefiltered specular env map
	m_prefilteredSpecEnvMap.Init(device, DXGI_FORMAT_R16G16B16A16_FLOAT, SPEC_CUBEMAP_RESOLUTION, SPEC_CUBEMAP_RESOLUTION, L"PrefilteredSpecEnvMap",
	                             kRenderTargetCubemap | kRenderTargetWithMips | kRenderTargetAllowUAV);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.Texture2DArray.ArraySize = 6;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.PlaneSlice = 0;
	m_prefilteredSpecEnvMapUAVs.resize(m_prefilteredSpecEnvMap.m_mipLevels);
	for (size_t i = 0; i < m_prefilteredSpecEnvMapUAVs.size(); i++)
	{
		uavDesc.Texture2DArray.MipSlice = (uint32_t)i;
		m_prefilteredSpecEnvMapUAVs[i] = device->CreateUAV(m_prefilteredSpecEnvMap.texture, &uavDesc);
	}

	// brdf lut
	m_brdfLut.Init(device, DXGI_FORMAT_R16G16B16A16_FLOAT, BRDF_LUT_SIZE, BRDF_LUT_SIZE, L"BRDF Lut", kRenderTargetAllowUAV);

	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;
	m_brdfLutUav = device->CreateUAV(m_brdfLut.texture, &uavDesc);

	// prefiltered diffuse env map
	m_prefilteredDiffEnvMap.Init(device, DXGI_FORMAT_R16G16B16A16_FLOAT, DIFF_CUBEMAP_RESOLUTION, DIFF_CUBEMAP_RESOLUTION, L"PrefilteredDiffEnvMap",
	                             kRenderTargetCubemap | kRenderTargetAllowUAV);

	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	uavDesc.Texture2DArray.ArraySize = 6;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.MipSlice = 0;
	uavDesc.Texture2DArray.PlaneSlice = 0;
	m_prefilteredDiffEnvMapUAV = device->CreateUAV(m_prefilteredDiffEnvMap.texture, &uavDesc);

	return true;
}


void EnvMapFilter::Shutdown()
{
	m_device->DeletePSO(m_envMapSpecPrefilter);
	m_device->DeletePSO(m_brdfLutGen);
	m_device->DeletePSO(m_envMapDiffPrefilter);

	for (UAVHandle uav : m_prefilteredSpecEnvMapUAVs)
		m_device->DestroyUAV(uav);
	m_prefilteredSpecEnvMap.Release(m_device);

	m_device->DestroyUAV(m_brdfLutUav);
	m_brdfLut.Release(m_device);

	m_device->DestroyUAV(m_prefilteredDiffEnvMapUAV);
	m_prefilteredDiffEnvMap.Release(m_device);
}


bool EnvMapFilter::ReloadShaders()
{
	m_device->DeletePSO(m_envMapSpecPrefilter);
	m_device->DeletePSO(m_brdfLutGen);
	m_device->DeletePSO(m_envMapDiffPrefilter);

	ComputePipelineStateDesc specPrefilterDesc;
	specPrefilterDesc.name = L"SpecEnvMapPrefilter";
	specPrefilterDesc.cs = L"envmapprefilter.hlsl";
	specPrefilterDesc.defines.push_back(L"SPECULAR");
	m_envMapSpecPrefilter = m_device->CreatePSO(specPrefilterDesc);

	ComputePipelineStateDesc lutGenDesc;
	lutGenDesc.name = L"BRDFLutGenerator";
	lutGenDesc.cs = L"brdflutgen.hlsl";
	m_brdfLutGen = m_device->CreatePSO(lutGenDesc);

	ComputePipelineStateDesc diffPrefilterDesc;
	diffPrefilterDesc.name = L"DiffEnvMapPrefilter";
	diffPrefilterDesc.cs = L"envmapprefilter.hlsl";
	diffPrefilterDesc.defines.push_back(L"DIFFUSE");
	m_envMapDiffPrefilter = m_device->CreatePSO(diffPrefilterDesc);

	return m_envMapSpecPrefilter && m_brdfLutGen && m_envMapDiffPrefilter;
}


void EnvMapFilter::FilterEnvMap(ID3D12GraphicsCommandList* cmdList, SRVHandle envmap)
{
	PIXScopedEvent(cmdList, 0, "FilterEnvMap");
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	D3D12_RESOURCE_STATES finalState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	if (!m_envMapSpecPrefilter || !m_brdfLutGen || !m_envMapDiffPrefilter)
	{
		m_prefilteredSpecEnvMap.TransitionTo(finalState, barriers);
		m_brdfLut.TransitionTo(finalState, barriers);
		m_prefilteredDiffEnvMap.TransitionTo(finalState, barriers);
		cmdList->ResourceBarrier(barriers.size(), barriers.data());
		return;
	}

	m_prefilteredSpecEnvMap.TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, barriers);
	m_brdfLut.TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, barriers);
	m_prefilteredDiffEnvMap.TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, barriers);
	cmdList->ResourceBarrier(barriers.size(), barriers.data());
	barriers.clear();

	// prefilter spec
	cmdList->SetPipelineState(m_envMapSpecPrefilter);
	for (uint32_t mip = 0; mip < m_prefilteredSpecEnvMap.m_mipLevels; mip++)
	{
		uint32_t subresourceIdx = ComputeSubresourceIndex(mip, 0, m_prefilteredSpecEnvMap.m_mipLevels);
		m_prefilteredSpecEnvMap.TransitionTo(D3D12_RESOURCE_STATE_UNORDERED_ACCESS, barriers, subresourceIdx);

		PrefilterConstants prefilterConsts;
		prefilterConsts.PrefilteredEnvMapUAVIdx = m_prefilteredSpecEnvMapUAVs[mip].idx;
		prefilterConsts.MipIndex = mip;
		prefilterConsts.MipsNumber = m_prefilteredSpecEnvMap.m_mipLevels;
		D3D12_GPU_VIRTUAL_ADDRESS gpuCbAddr = m_device->UpdateConstantBuffer(&prefilterConsts, sizeof(prefilterConsts));

		cmdList->SetComputeRootConstantBufferView(1, gpuCbAddr);
		uint32_t groupsNum = (CalcMipSize(SPEC_CUBEMAP_RESOLUTION, mip) + THREAD_GROUP_SIZE - 1) / 32;
		cmdList->Dispatch(groupsNum, groupsNum, 6);
	}

	// generate brdf lut
	LutGenConstants lutGenConsts;
	lutGenConsts.BRDFLutUAVIdx = m_brdfLutUav.idx;
	D3D12_GPU_VIRTUAL_ADDRESS gpuCbAddr = m_device->UpdateConstantBuffer(&lutGenConsts, sizeof(lutGenConsts));

	cmdList->SetComputeRootConstantBufferView(1, gpuCbAddr);
	cmdList->SetPipelineState(m_brdfLutGen);
	cmdList->Dispatch(BRDF_LUT_SIZE / THREAD_GROUP_SIZE, BRDF_LUT_SIZE / THREAD_GROUP_SIZE, 1);

	// prefilter diff
	PrefilterConstants prefilterConsts;
	prefilterConsts.PrefilteredEnvMapUAVIdx = m_prefilteredDiffEnvMapUAV.idx;
	prefilterConsts.MipIndex = 0;
	prefilterConsts.MipsNumber = m_prefilteredDiffEnvMap.m_mipLevels;
	gpuCbAddr = m_device->UpdateConstantBuffer(&prefilterConsts, sizeof(prefilterConsts));

	cmdList->SetComputeRootConstantBufferView(1, gpuCbAddr);
	cmdList->SetPipelineState(m_envMapDiffPrefilter);
	uint32_t groupsNum = (DIFF_CUBEMAP_RESOLUTION + THREAD_GROUP_SIZE - 1) / 32;
	cmdList->Dispatch(groupsNum, groupsNum, 6);

	m_prefilteredSpecEnvMap.TransitionTo(finalState, barriers);
	m_brdfLut.TransitionTo(finalState, barriers);
	m_prefilteredDiffEnvMap.TransitionTo(finalState, barriers);
	cmdList->ResourceBarrier(barriers.size(), barriers.data());
}
