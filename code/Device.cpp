#include "Precompiled.h"
#include "Device.h"
#include "ShaderCompiler.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


static bool IsCompressedFormat(DXGI_FORMAT format)
{
	switch (format)
	{
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return true;
		default:
			break;
	}
	return false;
}


bool DescriptorStorageHeap::Init(ID3D12Device5* device, const D3D12_DESCRIPTOR_HEAP_DESC& desc)
{
	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateDescriptorHeap failed: %x\n", hr);
		return false;
	}

	uint32_t masksNum = (desc.NumDescriptors + 63) / 64;
	m_masks.resize(masksNum);
	for (uint32_t i = 0; i < masksNum; i++)
		m_masks[i] = ~UINT64_C(0);

	m_descriptorSize = device->GetDescriptorHandleIncrementSize(desc.Type);

	return true;
}


void DescriptorStorageHeap::Shutdown()
{
	SafeRelease(m_heap);
}


std::tuple<DescriptorIndex, CpuDescriptorHandle> DescriptorStorageHeap::AllocateDescriptor()
{
	for (uint32_t i = 0; i < m_masks.size(); i++)
	{
		uint64_t& mask = m_masks[i];
		if (mask == 0)
			continue;

		DWORD bitIndex;
		BitScanForward64(&bitIndex, mask);
		mask &= ~(UINT64_C(1) << bitIndex);

		uint32_t descriptorIndex = i * 64 + bitIndex;
		CpuDescriptorHandle handle = m_heap->GetCPUDescriptorHandleForHeapStart().ptr + descriptorIndex * m_descriptorSize;
		return std::make_tuple(descriptorIndex, handle);
	}
	Assert(false);
	return std::make_tuple(DescriptorIndex(), CpuDescriptorHandle());
}


void DescriptorStorageHeap::FreeDescriptor(DescriptorIndex descriptorIndex)
{
	if (!descriptorIndex.IsValid())
		return;

	uint32_t maskIndex = descriptorIndex.idx / 64;
	uint32_t bitIndex = descriptorIndex.idx % 64;
	uint64_t& mask = m_masks[maskIndex];
	uint64_t bit = UINT64_C(1) << bitIndex;
	Assert((mask & bit) == 0);
	mask |= bit;
}


void DescriptorStorageHeap::FreeDescriptor(CpuDescriptorHandle handle)
{
	if (!handle.IsValid())
		return;

	uint32_t descriptorIndex = (uint32_t)(handle.ptr - m_heap->GetCPUDescriptorHandleForHeapStart().ptr) / m_descriptorSize;
	FreeDescriptor(DescriptorIndex(descriptorIndex));
}


bool CommandQueue::Init(ID3D12Device5* device, D3D12_COMMAND_LIST_TYPE type)
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.NodeMask = 0;
	HRESULT hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmdQueue));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateCommandQueue failed: %x\n", hr);
		return false;
	}

	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateFence failed: %x\n", hr);
		return false;
	}
	m_fence->Signal(m_lastCompletedFenceVal);

	m_fenceEvent = CreateEvent(nullptr, false, false, nullptr);
	if (m_fenceEvent == nullptr)
	{
		LogStdErr("CreateEvent failed\n");
		return false;
	}

	return true;
}


void CommandQueue::Shutdown()
{
	CloseHandle(m_fenceEvent);
	SafeRelease(m_fence);
	SafeRelease(m_cmdQueue);
}


uint64_t CommandQueue::SignalFence()
{
	m_cmdQueue->Signal(m_fence, m_nextFenceVal);
	return m_nextFenceVal++;
}


void CommandQueue::WaitForFence(uint64_t fenceVal)
{
	PIXScopedEvent(0xffffff00, "WaitForFence");
	if (fenceVal <= m_lastCompletedFenceVal)
		return;

	m_fence->SetEventOnCompletion(fenceVal, m_fenceEvent);
	WaitForSingleObject(m_fenceEvent, INFINITE);
	m_lastCompletedFenceVal = fenceVal;
}


void CommandQueue::WaitForIdle()
{
	WaitForFence(SignalFence());
}


bool Device::Init(uint32_t adaptedIdx)
{
	PIXScopedEvent(0, "Device::Init");
	UINT dxgiFactoryFlag = 0;
#if _DEBUG
	ComPtr<ID3D12Debug> debugInterface;
	ComPtr<ID3D12Debug1> debugInterface1;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
	{
		debugInterface->EnableDebugLayer();
		/*if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface1))))
		    debugInterface1->SetEnableGPUBasedValidation(true);
		else
		    LogStdErr("Failed to enable GPU based validation\n");*/
	}
	else
		LogStdErr("Failed to enable debug layer\n");

	dxgiFactoryFlag = DXGI_CREATE_FACTORY_DEBUG;
#endif

	HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlag, IID_PPV_ARGS(&m_dxgiFactory));
	if (FAILED(hr))
	{
		LogStdErr("CreateDXGIFactory2 failed: %x\n", hr);
		return false;
	}

	ComPtr<IDXGIAdapter1> adapter;
	hr = m_dxgiFactory->EnumAdapters1(adaptedIdx, &adapter);
	if (FAILED(hr))
	{
		LogStdErr("IDXGIFactory4::EnumAdapters1 failed: %x\n", hr);
		return false;
	}

	DXGI_ADAPTER_DESC1 adapterDesc;
	adapter->GetDesc1(&adapterDesc);
	LogStdOut("Adapter info:\n\tDescription: %ws\n", adapterDesc.Description);

	hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3d12Device));
	if (FAILED(hr))
	{
		LogStdErr("D3D12CreateDevice failed: %x\n", hr);
		return false;
	}

#if _DEBUG
	ID3D12InfoQueue* pInfoQueue = nullptr;
	if (SUCCEEDED(m_d3d12Device->QueryInterface(IID_PPV_ARGS(&pInfoQueue))))
	{
		D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};

		D3D12_INFO_QUEUE_FILTER newFilter = {};
		newFilter.DenyList.NumSeverities = _countof(severities);
		newFilter.DenyList.pSeverityList = severities;

		pInfoQueue->PushStorageFilter(&newFilter);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		pInfoQueue->Release();
	}
#endif

	if (!m_cmdQueue.Init(m_d3d12Device, D3D12_COMMAND_LIST_TYPE_DIRECT))
		return false;

	for (uint32_t i = 0; i < kFramesNum; i++)
	{
		hr = m_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAllocators[i]));
		if (FAILED(hr))
		{
			LogStdErr("ID3D12Device::CreateCommandAllocator failed: %x\n", hr);
			return false;
		}
	}

	// hr = m_d3d12Device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_cmdList));
	hr = m_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAllocators[0], nullptr, IID_PPV_ARGS(&m_cmdList));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateCommandList1 failed: %x\n", hr);
		return false;
	}
	m_cmdList->Close();

	if (!CreateRootSignature())
		return false;

	D3D12_RESOURCE_DESC constBufDesc = {};
	constBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	constBufDesc.Alignment = 0;
	constBufDesc.Width = kConstBufferSize;
	constBufDesc.Height = 1;
	constBufDesc.DepthOrArraySize = 1;
	constBufDesc.MipLevels = 1;
	constBufDesc.Format = DXGI_FORMAT_UNKNOWN;
	constBufDesc.SampleDesc = {1, 0};
	constBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	constBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	hr = m_d3d12Device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &constBufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
	                                            IID_PPV_ARGS(&m_constBufferStream));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateCommittedResource failed: %x\n", hr);
		return false;
	}

	m_constBufferData.reserve(kConstBufferSize);

	if (!CreateDescriptorHeaps())
		return false;

	if (!InitTransferFunctionality())
		return false;

	return true;
}


void Device::Shutdown()
{
	m_cmdQueue.WaitForIdle();

	for (ID3D12Object* obj : m_deleteQueue)
		SafeRelease(obj);
	m_deleteQueue.clear();

	SafeRelease(m_gpuCbvSrvUavHeap);
	m_cbvSrvUavStorageHeap.Shutdown();
	m_rtvStorageHeap.Shutdown();
	m_dsvStorageHeap.Shutdown();
	SafeRelease(m_constBufferStream);
	SafeRelease(m_rootSig);
	SafeRelease(m_cmdList);
	for (ID3D12CommandAllocator*& cmdAlloc : m_cmdAllocators)
		SafeRelease(cmdAlloc);
	m_cmdQueue.Shutdown();

	SafeRelease(m_transferBuffer);
	SafeRelease(m_transferCmdList);
	SafeRelease(m_transferCmdAllocator);
	m_transferQueue.Shutdown();

#if defined(_DEBUG)
	ID3D12DebugDevice* debugInterface;
	if (SUCCEEDED(m_d3d12Device->QueryInterface(&debugInterface)))
	{
		debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		debugInterface->Release();
	}
#endif

	SafeRelease(m_d3d12Device);
	SafeRelease(m_dxgiFactory);
}


bool Device::CreateSwapChain(const DXGI_SWAP_CHAIN_DESC1& desc, HWND windowHwnd, IDXGISwapChain3** swapChain)
{
	HRESULT hr = m_dxgiFactory->CreateSwapChainForHwnd(m_cmdQueue.GetD3D12Queue(), windowHwnd, &desc, nullptr, nullptr, (IDXGISwapChain1**)swapChain);
	if (FAILED(hr))
	{
		LogStdErr("IDXGIFactory4::CreateSwapChainForHwnd failed: %x\n", hr);
		return false;
	}
	return true;
}


ID3D12PipelineState* Device::CreatePSO(const GfxPipelineStateDesc& desc)
{
	if (!desc.name || wcslen(desc.name) == 0)
	{
		LogStdErr("Name for PSO is required\n");
		return false;
	}

	PIXScopedEvent(0, "CreatePSO '%S'", desc.name);
	D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12Desc = {};
	d3d12Desc.pRootSignature = m_rootSig;
	d3d12Desc.BlendState = desc.BlendState;
	d3d12Desc.SampleMask = 0xffffffff;
	d3d12Desc.RasterizerState = desc.RasterizerState;
	d3d12Desc.DepthStencilState = desc.DepthStencilState;
	d3d12Desc.InputLayout = desc.InputLayout;
	d3d12Desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	d3d12Desc.PrimitiveTopologyType = desc.PrimitiveTopologyType;
	d3d12Desc.NumRenderTargets = desc.NumRenderTargets;
	memcpy(d3d12Desc.RTVFormats, desc.RTVFormats, sizeof(desc.RTVFormats));
	d3d12Desc.DSVFormat = desc.DSVFormat;
	d3d12Desc.SampleDesc = desc.SampleDesc;

	ComPtr<IDxcBlob> blobs[kShaderTypesCount] = {};
	if (desc.vs)
	{
		IDxcBlob* blob = CompileShader(desc.vs, kShaderVertex, desc.defines);
		if (!blob)
			return nullptr;
		d3d12Desc.VS = {blob->GetBufferPointer(), blob->GetBufferSize()};
		blobs[kShaderVertex] = blob;
	}
	if (desc.gs)
	{
		IDxcBlob* blob = CompileShader(desc.gs, kShaderGeometry, desc.defines);
		if (!blob)
			return nullptr;
		d3d12Desc.GS = {blob->GetBufferPointer(), blob->GetBufferSize()};
		blobs[kShaderGeometry] = blob;
	}
	if (desc.ps)
	{
		IDxcBlob* blob = CompileShader(desc.ps, kShaderPixel, desc.defines);
		if (!blob)
			return nullptr;
		d3d12Desc.PS = {blob->GetBufferPointer(), blob->GetBufferSize()};
		blobs[kShaderPixel] = blob;
	}

	ID3D12PipelineState* pso = nullptr;
	HRESULT hr = m_d3d12Device->CreateGraphicsPipelineState(&d3d12Desc, IID_PPV_ARGS(&pso));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateGraphicsPipelineState failed: %x\n", hr);
		return nullptr;
	}
	pso->SetName(desc.name);
	return pso;
}


ID3D12PipelineState* Device::CreatePSO(const ComputePipelineStateDesc& desc)
{
	if (!desc.name || wcslen(desc.name) == 0)
	{
		LogStdErr("Name for PSO is required\n");
		return false;
	}

	PIXScopedEvent(0, "CreatePSO '%S'", desc.name);
	D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12Desc = {};
	d3d12Desc.pRootSignature = m_rootSig;
	ComPtr<IDxcBlob> blob = CompileShader(desc.cs, kShaderCompute, desc.defines);
	if (!blob)
		return nullptr;
	d3d12Desc.CS = {blob->GetBufferPointer(), blob->GetBufferSize()};

	ID3D12PipelineState* pso = nullptr;
	HRESULT hr = m_d3d12Device->CreateComputePipelineState(&d3d12Desc, IID_PPV_ARGS(&pso));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateGraphicsPipelineState failed: %x\n", hr);
		return nullptr;
	}
	pso->SetName(desc.name);
	return pso;
}


void Device::DeletePSO(ID3D12PipelineState* pso)
{
	if (!pso)
		return;
	m_deleteQueue.push_back(pso);
}


CBVHandle Device::CreateCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc)
{
	auto [idx, handle] = m_cbvSrvUavStorageHeap.AllocateDescriptor();
	m_d3d12Device->CreateConstantBufferView(desc, handle);
	return idx;
}


void Device::DestroyCBV(CBVHandle handle)
{
	m_cbvSrvUavStorageHeap.FreeDescriptor(handle);
}


SRVHandle Device::CreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc)
{
	auto [idx, handle] = m_cbvSrvUavStorageHeap.AllocateDescriptor();
	m_d3d12Device->CreateShaderResourceView(resource, desc, handle);
	return idx;
}


void Device::DestroySRV(SRVHandle handle)
{
	m_cbvSrvUavStorageHeap.FreeDescriptor(handle);
}


UAVHandle Device::CreateUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc)
{
	auto [idx, handle] = m_cbvSrvUavStorageHeap.AllocateDescriptor();
	m_d3d12Device->CreateUnorderedAccessView(resource, nullptr, desc, handle);
	return idx;
}


void Device::DestroyUAV(UAVHandle handle)
{
	m_cbvSrvUavStorageHeap.FreeDescriptor(handle);
}


RTVHandle Device::CreateRTV(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc)
{
	auto [idx, handle] = m_rtvStorageHeap.AllocateDescriptor();
	m_d3d12Device->CreateRenderTargetView(resource, desc, handle);
	return handle;
}


void Device::DestroyRTV(RTVHandle handle)
{
	m_rtvStorageHeap.FreeDescriptor(handle);
}


DSVHandle Device::CreateDSV(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc)
{
	auto [idx, handle] = m_dsvStorageHeap.AllocateDescriptor();
	m_d3d12Device->CreateDepthStencilView(resource, desc, handle);
	return handle;
}


void Device::DestroyDSV(DSVHandle handle)
{
	m_dsvStorageHeap.FreeDescriptor(handle);
}


void Device::DestroyResource(ID3D12Object* resource)
{
	if (!resource)
		return;
	m_deleteQueue.push_back(resource);
}


void Device::BeginFrame()
{
	m_constBufferData.clear();
	m_cmdAllocators[m_curFrameIdx]->Reset();
	m_cmdList->Reset(m_cmdAllocators[m_curFrameIdx], nullptr);
	PIXBeginEvent(m_cmdList, 0xffffff00, "Frame %u", m_curFrameIdx);
	m_cmdList->SetDescriptorHeaps(1, &m_gpuCbvSrvUavHeap);
	m_cmdList->SetGraphicsRootSignature(m_rootSig);
	m_cmdList->SetComputeRootSignature(m_rootSig);
	m_cmdList->SetGraphicsRootDescriptorTable(kCBVSlotsNum, m_gpuCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
	m_cmdList->SetComputeRootDescriptorTable(kCBVSlotsNum, m_gpuCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
}


D3D12_GPU_VIRTUAL_ADDRESS Device::UpdateConstantBuffer(const void* data, uint32_t size)
{
	D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = m_constBufferStream->GetGPUVirtualAddress();
	size_t oldSize = m_constBufferData.size();
	size_t newSize = m_constBufferData.size() + Align(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	m_constBufferData.resize(newSize);
	memcpy(m_constBufferData.data() + oldSize, data, size);
	return gpuAddr + oldSize;
}


void Device::EndFrame()
{
	PIXScopedEvent(0xffffff00, "Submit");
	if (m_prevFrameFenceVal != ~UINT64_C(0))
		m_cmdQueue.WaitForFence(m_prevFrameFenceVal);

	// TODO optimize
	{
		PIXScopedEvent(0xffffff00, "CopyDescriptors");
		const D3D12_CPU_DESCRIPTOR_HANDLE dstHeapStart = m_gpuCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
		const D3D12_CPU_DESCRIPTOR_HANDLE srcHeapStart = m_cbvSrvUavStorageHeap.GetHeap()->GetCPUDescriptorHandleForHeapStart();
		m_d3d12Device->CopyDescriptorsSimple(kCbvSrvUavHeapSize, dstHeapStart, srcHeapStart, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	if (m_constBufferData.size())
	{
		void* mapAddr = nullptr;
		m_constBufferStream->Map(0, nullptr, &mapAddr);
		memcpy(mapAddr, m_constBufferData.data(), m_constBufferData.size());
		D3D12_RANGE writeRange = {0, m_constBufferData.size()};
		m_constBufferStream->Unmap(0, &writeRange);
		m_constBufferData.clear();
	}

	PIXEndEvent(m_cmdList);
	m_cmdList->Close();

	ID3D12CommandList* cmdList = m_cmdList;
	m_cmdQueue.GetD3D12Queue()->ExecuteCommandLists(1, &cmdList);
	m_prevFrameFenceVal = m_cmdQueue.SignalFence();
	m_curFrameIdx = (m_curFrameIdx + 1) % kFramesNum;

	for (ID3D12Object* obj : m_deleteQueue)
		SafeRelease(obj);
	m_deleteQueue.clear();
}


void Device::BeginTransfer()
{
	if (m_transferInProgress == 0)
	{
		m_transferCmdList->Reset(m_transferCmdAllocator, nullptr);
		PIXBeginEvent(m_transferCmdList, 0xffffff00, "Transfer");
	}
	m_transferInProgress++;
}


void Device::UploadTextureSubresource(ID3D12Resource* texture, uint32_t mip, uint32_t slice, const void* data, uint32_t rowPitch)
{
	PIXScopedEvent(0xffffff00, "UploadTextureSubresource");
	Assert(m_transferInProgress > 0);

	D3D12_RESOURCE_DESC desc = texture->GetDesc();
	Assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);

	uint32_t mipWidth = CalcMipSize((uint32_t)desc.Width, mip);
	uint32_t mipHeight = CalcMipSize(desc.Height, mip);
	// uint32_t mipDepth = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? CalcMipSize(desc.DepthOrArraySize, mip) : 1;
	uint32_t numRows = mipHeight;
	if (IsCompressedFormat(desc.Format))
	{
		mipWidth = Align(mipWidth, 4);
		mipHeight = Align(mipHeight, 4);
		numRows = mipHeight / 4;
	}

	uint32_t uploadPitch = Align(rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	uint32_t uploadSize = uploadPitch * numRows;
	if (uploadSize > m_transferBufferCapacity)
	{
		FlushTransfer();
		m_transferCmdList->Reset(m_transferCmdAllocator, nullptr);
		PIXBeginEvent(m_transferCmdList, 0xffffff00, "Transfer");
		CreateTransferBuffer(CeilPowerOf2(uploadSize));
	}
	if (m_transferBufferCapacity - m_transferBufferSize < uploadSize)
	{
		FlushTransfer();
		m_transferCmdList->Reset(m_transferCmdAllocator, nullptr);
		PIXBeginEvent(m_transferCmdList, 0xffffff00, "Transfer");
	}

	uint8_t* dstPtr = m_transferBufferMapped + m_transferBufferSize;
	if (uploadPitch == rowPitch)
	{
		memcpy(dstPtr, data, uploadSize);
	}
	else
	{
		for (uint32_t y = 0; y < numRows; y++)
			memcpy(dstPtr + y * uploadPitch, (const uint8_t*)data + y * rowPitch, rowPitch);
	}

	D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
	srcLocation.pResource = m_transferBuffer;
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcLocation.PlacedFootprint.Offset = m_transferBufferSize;
	srcLocation.PlacedFootprint.Footprint.Format = desc.Format;
	srcLocation.PlacedFootprint.Footprint.Width = mipWidth;
	srcLocation.PlacedFootprint.Footprint.Height = mipHeight;
	srcLocation.PlacedFootprint.Footprint.Depth = 1;
	srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

	D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
	dstLocation.pResource = texture;
	dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstLocation.SubresourceIndex = ComputeSubresourceIndex(mip, slice, desc.MipLevels);

	m_transferCmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
	m_transferBufferSize += Align(uploadSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
}


uint8_t* Device::PrepareForBufferUpload(uint32_t size)
{
	if (size > m_transferBufferCapacity)
	{
		FlushTransfer();
		m_transferCmdList->Reset(m_transferCmdAllocator, nullptr);
		PIXBeginEvent(m_transferCmdList, 0xffffff00, "Transfer");
		CreateTransferBuffer(CeilPowerOf2(size));
	}
	if (m_transferBufferCapacity - m_transferBufferSize < size)
	{
		FlushTransfer();
		m_transferCmdList->Reset(m_transferCmdAllocator, nullptr);
		PIXBeginEvent(m_transferCmdList, 0xffffff00, "Transfer");
	}

	m_bufferUploadOffset = m_transferBufferSize;
	m_bufferUploadSize = size;

	return m_transferBufferMapped + m_transferBufferSize;
}


void Device::UploadBuffer(ID3D12Resource* buffer, uint64_t dstOffset)
{
	m_transferCmdList->CopyBufferRegion(buffer, dstOffset, m_transferBuffer, m_bufferUploadOffset, m_bufferUploadSize);
	m_transferBufferSize += m_bufferUploadSize;
}


void Device::EndTransfer()
{
	if (--m_transferInProgress > 0)
		return;
	FlushTransfer();
}


bool Device::CreateRootSignature()
{
	std::vector<D3D12_ROOT_PARAMETER> rootParams;
	D3D12_ROOT_PARAMETER param;
	param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	for (uint32_t i = 0; i < kCBVSlotsNum; i++)
	{
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		param.Descriptor.RegisterSpace = 0;
		param.Descriptor.ShaderRegister = i;
		rootParams.push_back(param);
	}

	std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;
	D3D12_DESCRIPTOR_RANGE range;
	// Texture2Ds
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors = UINT_MAX;
	range.BaseShaderRegister = 0;
	range.RegisterSpace = 0;
	range.OffsetInDescriptorsFromTableStart = 0;
	descriptorRanges.push_back(range);
	// TextureCubes
	range.RegisterSpace = 1;
	descriptorRanges.push_back(range);
	// Textures2DArray
	range.RegisterSpace = 2;
	descriptorRanges.push_back(range);
	// RWTextures2D
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	range.RegisterSpace = 3;
	descriptorRanges.push_back(range);
	// RWTextures2DArray
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	range.RegisterSpace = 4;
	descriptorRanges.push_back(range);
	// Buffers
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.RegisterSpace = 5;
	descriptorRanges.push_back(range);

	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.DescriptorTable.NumDescriptorRanges = (uint32_t)descriptorRanges.size();
	param.DescriptorTable.pDescriptorRanges = descriptorRanges.data();
	rootParams.push_back(param);

	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.RegisterSpace = 0;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	samplerDesc.ShaderRegister = 0;
	staticSamplers.push_back(samplerDesc);

	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.ShaderRegister = 1;
	staticSamplers.push_back(samplerDesc);

	samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	samplerDesc.MaxAnisotropy = 0;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
	samplerDesc.MaxLOD = 0.0f;
	samplerDesc.ShaderRegister = 2;
	staticSamplers.push_back(samplerDesc);

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.NumParameters = (uint32_t)rootParams.size();
	rootSigDesc.pParameters = rootParams.data();
	rootSigDesc.NumStaticSamplers = (uint32_t)staticSamplers.size();
	rootSigDesc.pStaticSamplers = staticSamplers.data();

	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> errorBlob;
	HRESULT hr;
	hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, blob.GetAddressOf(), errorBlob.GetAddressOf());
	if (FAILED(hr))
	{
		LogStdErr("D3D12SerializeRootSignature failed: %x\n", hr);
		if (errorBlob && errorBlob->GetBufferPointer())
			LogStdErr("%s\n", (const char*)errorBlob->GetBufferPointer());
		return false;
	}

	hr = m_d3d12Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSig));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateRootSignature failed: %x\n", hr);
		return false;
	}

	return true;
}


bool Device::CreateDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = kCbvSrvUavHeapSize;
	m_cbvSrvUavStorageHeap.Init(m_d3d12Device, desc);

	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.NumDescriptors = kRtvDsvHeapSize;
	m_rtvStorageHeap.Init(m_d3d12Device, desc);

	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.NumDescriptors = kRtvDsvHeapSize;
	m_dsvStorageHeap.Init(m_d3d12Device, desc);

	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = kCbvSrvUavHeapSize;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HRESULT hr = m_d3d12Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_gpuCbvSrvUavHeap));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateDescriptorHeap failed: %x\n", hr);
		return false;
	}

	return true;
}


bool Device::InitTransferFunctionality()
{
	D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_COPY;

	if (!m_transferQueue.Init(m_d3d12Device, type))
		return false;

	HRESULT hr = m_d3d12Device->CreateCommandAllocator(type, IID_PPV_ARGS(&m_transferCmdAllocator));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateCommandAllocator failed: %x\n", hr);
		return false;
	}

	// hr = m_d3d12Device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_transferCmdList));
	hr = m_d3d12Device->CreateCommandList(0, type, m_transferCmdAllocator, nullptr, IID_PPV_ARGS(&m_transferCmdList));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateCommandList1 failed: %x\n", hr);
		return false;
	}
	m_transferCmdList->Close();

	const uint32_t kInitialTransferBufCapacity = 8 * 1024 * 1024;
	if (!CreateTransferBuffer(kInitialTransferBufCapacity))
		return false;

	return true;
}


bool Device::CreateTransferBuffer(uint32_t size)
{
	if (m_transferBuffer)
	{
		m_transferBuffer->Unmap(0, nullptr);
		m_deleteQueue.push_back(m_transferBuffer);
		m_transferBuffer = nullptr;
	}

	m_transferBufferCapacity = size;

	D3D12_RESOURCE_DESC bufferDesc;
	memset(&bufferDesc, 0, sizeof(bufferDesc));
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = 0;
	bufferDesc.Width = m_transferBufferCapacity;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.SampleDesc.Quality = 0;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	HRESULT hr = m_d3d12Device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
	                                                    IID_PPV_ARGS(&m_transferBuffer));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateCommittedResource failed: %x\n", hr);
		return false;
	}

	m_transferBuffer->Map(0, nullptr, (void**)&m_transferBufferMapped);
	return true;
}


void Device::FlushTransfer()
{
	PIXEndEvent(m_transferCmdList);
	m_transferCmdList->Close();
	ID3D12CommandList* cmdList = m_transferCmdList;
	m_transferQueue.GetD3D12Queue()->ExecuteCommandLists(1, &cmdList);
	m_transferQueue.WaitForIdle();
	m_transferBufferSize = 0;
}
