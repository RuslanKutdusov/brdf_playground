#pragma once

#define USE_PIX 1
#include <pix3.h>

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;


template <class T>
void SafeRelease(T*& t)
{
	if (t)
		t->Release();
	t = nullptr;
}


template<typename T>
inline T Align(T t, uint32_t alignTo)
{
	T mask = alignTo - 1;
	return (t + mask) & ~mask;
}


inline uint32_t CeilPowerOf2(uint32_t x)
{
	x = x - 1;
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);
	return x + 1;
}


inline uint32_t ComputeSubresourceIndex(uint32_t mip, uint32_t slice, uint32_t mipLevelsNum)
{
	return mip + slice * mipLevelsNum;
}


inline uint32_t CalcMipSize(uint32_t mip0Size, uint32_t mipLevel)
{
	return std::max(1u, mip0Size >> mipLevel);
}


inline uint32_t ComputeMipLevelsNum(uint32_t width, uint32_t height)
{
	uint32_t size = std::max(width, height);
	if (!size)
		return 0;
	DWORD idx;
	BitScanReverse(&idx, size);
	return idx + 1;
}


struct DescriptorIndex
{
	uint32_t idx = ~0u;

	DescriptorIndex() = default;

	DescriptorIndex(uint32_t idx_) : idx(idx_)
	{
	}

	bool IsValid() const
	{
		return idx != ~0u;
	}

	void Invalidate()
	{
		idx = ~0u;
	}
};


struct CpuDescriptorHandle : public D3D12_CPU_DESCRIPTOR_HANDLE
{
	CpuDescriptorHandle()
	{
		Invalidate();
	}

	CpuDescriptorHandle(uint64_t ptr_)
	{
		ptr = ptr_;
	}

	bool IsValid() const
	{
		return ptr != ~UINT64_C(0);
	}

	void Invalidate()
	{
		ptr = ~UINT64_C(0);
	}
};


class DescriptorStorageHeap
{
public:
	bool Init(ID3D12Device5* device, const D3D12_DESCRIPTOR_HEAP_DESC& desc);
	void Shutdown();

	std::tuple<DescriptorIndex, CpuDescriptorHandle> AllocateDescriptor();
	void FreeDescriptor(DescriptorIndex descriptorIndex);
	void FreeDescriptor(CpuDescriptorHandle handle);

	ID3D12DescriptorHeap* GetHeap() const
	{
		return m_heap;
	}

private:
	ID3D12DescriptorHeap* m_heap = nullptr;
	uint32_t m_descriptorSize = 0;
	std::vector<uint64_t> m_masks;
};


struct CBVHandle : public DescriptorIndex
{
	CBVHandle() = default;
	CBVHandle(const DescriptorIndex& idx) : DescriptorIndex(idx)
	{
	}
};


struct SRVHandle : public DescriptorIndex
{
	SRVHandle() = default;
	SRVHandle(const DescriptorIndex& idx) : DescriptorIndex(idx)
	{
	}
};


struct UAVHandle : public DescriptorIndex
{
	UAVHandle() = default;
	UAVHandle(const DescriptorIndex& idx) : DescriptorIndex(idx)
	{
	}
};


struct RTVHandle : public CpuDescriptorHandle
{
	RTVHandle() = default;
	RTVHandle(const CpuDescriptorHandle& idx) : CpuDescriptorHandle(idx)
	{
	}
};


struct DSVHandle : public CpuDescriptorHandle
{
	DSVHandle() = default;
	DSVHandle(const CpuDescriptorHandle& idx) : CpuDescriptorHandle(idx)
	{
	}
};


struct GfxPipelineStateDesc
{
	const wchar_t* name = nullptr;
	D3D12_INPUT_LAYOUT_DESC InputLayout = {};
	D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	const wchar_t* vs = nullptr;
	const wchar_t* gs = nullptr;
	const wchar_t* ps = nullptr;
	std::vector<const wchar_t*> defines;
	D3D12_BLEND_DESC BlendState = {};
	D3D12_RASTERIZER_DESC RasterizerState = {};
	D3D12_DEPTH_STENCIL_DESC DepthStencilState = {};
	UINT NumRenderTargets = 0;
	DXGI_FORMAT RTVFormats[8] = {};
	DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;
	DXGI_SAMPLE_DESC SampleDesc = {1, 0};
};


struct ComputePipelineStateDesc
{
	const wchar_t* name = nullptr;
	const wchar_t* cs;
	std::vector<const wchar_t*> defines;
};


class CommandQueue
{
public:
	bool Init(ID3D12Device5* device, D3D12_COMMAND_LIST_TYPE type);
	void Shutdown();

	ID3D12CommandQueue* GetD3D12Queue() const
	{
		return m_cmdQueue;
	}

	uint64_t SignalFence();
	void WaitForFence(uint64_t fenceVal);
	void WaitForIdle();
private:
	ID3D12CommandQueue* m_cmdQueue = nullptr;
	ID3D12Fence* m_fence = nullptr;
	HANDLE m_fenceEvent = nullptr;
	uint64_t m_nextFenceVal = 1;
	uint64_t m_lastCompletedFenceVal = 0;
};


class Device
{
public:
	static const uint32_t kFramesNum = 2;
	static const uint32_t kCBVSlotsNum = 4;

	bool Init(uint32_t adaptedIdx);
	void Shutdown();

	bool CreateSwapChain(const DXGI_SWAP_CHAIN_DESC1& desc, HWND windowHwnd, IDXGISwapChain3** swapChain);

	ID3D12PipelineState* CreatePSO(const GfxPipelineStateDesc& desc);
	ID3D12PipelineState* CreatePSO(const ComputePipelineStateDesc& desc);
	void DeletePSO(ID3D12PipelineState* pso);

	CBVHandle CreateCBV(const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc);
	void DestroyCBV(CBVHandle handle);
	SRVHandle CreateSRV(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc);
	void DestroySRV(SRVHandle handle);
	UAVHandle CreateUAV(ID3D12Resource* resource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc);
	void DestroyUAV(UAVHandle handle);
	RTVHandle CreateRTV(ID3D12Resource* resource, const D3D12_RENDER_TARGET_VIEW_DESC* desc);
	void DestroyRTV(RTVHandle handle);
	DSVHandle CreateDSV(ID3D12Resource* resource, const D3D12_DEPTH_STENCIL_VIEW_DESC* desc);
	void DestroyDSV(DSVHandle handle);

	void DestroyResource(ID3D12Object* resource);

	ID3D12Device5* GetDevice() const;
	IDXGIFactory5* GetDXGIFactory() const;
	CommandQueue& GetCommandQueue();
	ID3D12GraphicsCommandList* GetCommandList() const;
	uint32_t GetFrameIdx() const;

	void BeginFrame();
	D3D12_GPU_VIRTUAL_ADDRESS UpdateConstantBuffer(const void* data, uint32_t size);
	void EndFrame();

	void BeginTransfer();
	void UploadTextureSubresource(ID3D12Resource* texture, uint32_t mip, uint32_t slice, const void* data, uint32_t rowPitch);
	uint8_t* PrepareForBufferUpload(uint32_t size);
	void UploadBuffer(ID3D12Resource* buffer, uint64_t dstOffset);
	void EndTransfer();

private:
	IDXGIFactory5* m_dxgiFactory = nullptr;
	ID3D12Device5* m_d3d12Device = nullptr;

	std::vector<ID3D12Object*> m_deleteQueue;

	CommandQueue m_cmdQueue;

	uint32_t m_curFrameIdx = 0;
	ID3D12CommandAllocator* m_cmdAllocators[kFramesNum] = {};
	ID3D12GraphicsCommandList* m_cmdList = nullptr;
	uint64_t m_prevFrameFenceVal = ~UINT64_C(0);

	CommandQueue m_transferQueue;
	ID3D12CommandAllocator* m_transferCmdAllocator = nullptr;
	ID3D12GraphicsCommandList* m_transferCmdList = nullptr;
	ID3D12Resource* m_transferBuffer = nullptr;
	uint32_t m_transferBufferSize = 0;
	uint32_t m_transferBufferCapacity = 0;
	uint8_t* m_transferBufferMapped = nullptr;
	uint32_t m_transferInProgress = 0;
	uint64_t m_bufferUploadOffset = 0;
	uint32_t m_bufferUploadSize = 0;

	ID3D12RootSignature* m_rootSig = nullptr;

	static const uint32_t kConstBufferSize = 1 * 1024 * 1024;
	ID3D12Resource* m_constBufferStream = nullptr;
	std::vector<uint8_t> m_constBufferData;

	static const uint32_t kCbvSrvUavHeapSize = 1024;
	static const uint32_t kRtvDsvHeapSize = 128;
	DescriptorStorageHeap m_cbvSrvUavStorageHeap;
	DescriptorStorageHeap m_rtvStorageHeap;
	DescriptorStorageHeap m_dsvStorageHeap;

	ID3D12DescriptorHeap* m_gpuCbvSrvUavHeap = nullptr;

	bool CreateRootSignature();
	bool CreateDescriptorHeaps();
	bool InitTransferFunctionality();
	bool CreateTransferBuffer(uint32_t size);
	void FlushTransfer();
};


inline ID3D12Device5* Device::GetDevice() const
{
	return m_d3d12Device;
}


inline IDXGIFactory5* Device::GetDXGIFactory() const
{
	return m_dxgiFactory;
}


inline CommandQueue& Device::GetCommandQueue()
{
	return m_cmdQueue;
}


inline ID3D12GraphicsCommandList* Device::GetCommandList() const
{
	return m_cmdList;
}


inline uint32_t Device::GetFrameIdx() const
{
	return m_curFrameIdx;
}


inline D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource,
                                                D3D12_RESOURCE_STATES stateBefore,
                                                D3D12_RESOURCE_STATES stateAfter,
                                                uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                                D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
{
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = flags;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = stateBefore;
	barrier.Transition.StateAfter = stateAfter;
	barrier.Transition.Subresource = subresource;
	return barrier;
}


inline D3D12_RASTERIZER_DESC GetDefaultRasterizerDesc()
{
	D3D12_RASTERIZER_DESC desc;
	desc.FillMode = D3D12_FILL_MODE_SOLID;
	desc.CullMode = D3D12_CULL_MODE_BACK;
	desc.FrontCounterClockwise = false;
	desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	desc.DepthClipEnable = true;
	desc.MultisampleEnable = false;
	desc.AntialiasedLineEnable = false;
	desc.ForcedSampleCount = 0;
	desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	return desc;
}


inline D3D12_BLEND_DESC GetDefaultBlendDesc()
{
	D3D12_BLEND_DESC desc;
	desc.AlphaToCoverageEnable = false;
	desc.IndependentBlendEnable = false;
	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
	    false,
	    false,
	    D3D12_BLEND_ONE,
	    D3D12_BLEND_ZERO,
	    D3D12_BLEND_OP_ADD,
	    D3D12_BLEND_ONE,
	    D3D12_BLEND_ZERO,
	    D3D12_BLEND_OP_ADD,
	    D3D12_LOGIC_OP_NOOP,
	    D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		desc.RenderTarget[i] = defaultRenderTargetBlendDesc;
	return desc;
}


inline D3D12_DEPTH_STENCIL_DESC GetDefaultDepthStencilDesc()
{
	D3D12_DEPTH_STENCIL_DESC desc;
	desc.DepthEnable = true;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	desc.StencilEnable = false;
	desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
	desc.FrontFace = defaultStencilOp;
	desc.BackFace = defaultStencilOp;
	return desc;
}


inline void SetViewportAndScissorRect(ID3D12GraphicsCommandList* cmdList, uint32_t width, uint32_t height)
{
	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	D3D12_RECT scissorRect = {0, 0, (LONG)width, (LONG)height};

	float black[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissorRect);
}