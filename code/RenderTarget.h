#pragma once


enum ERenderTargetFlags
{
	kRenderTargetNone = 0,
	kRenderTargetCubemap = 1 << 0,
	kRenderTargetWithMips = 1 << 1,
	kRenderTargetAllowRTV_DSV = 1 << 2,
	kRenderTargetAllowUAV = 1 << 3,
};
DEFINE_ENUM_FLAG_OPERATORS(ERenderTargetFlags)


struct RenderTarget
{
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	uint32_t m_arraySize = 0;
	uint32_t m_mipLevels;
	DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
	ERenderTargetFlags m_flags = kRenderTargetNone;

	ID3D12Resource* texture = nullptr;
	RTVHandle rtv;
	DSVHandle dsv;
	SRVHandle srv;
	std::vector<D3D12_RESOURCE_STATES> subresourcesState;

	bool Init(Device* device, DXGI_FORMAT format, uint32_t width, uint32_t height, const wchar_t* name, ERenderTargetFlags flags = kRenderTargetNone);
	void Release(Device* device);
	bool TransitionTo(D3D12_RESOURCE_STATES newState,
	                  std::vector<D3D12_RESOURCE_BARRIER>& barriers,
	                  uint32_t subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
};
