#include "Precompiled.h"
#include "RenderTarget.h"


static DXGI_FORMAT GetDepthSRVFormat(DXGI_FORMAT depthFormat)
{
	switch (depthFormat)
	{
		case DXGI_FORMAT_D16_UNORM:
			return DXGI_FORMAT_R16_UNORM;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		default:
			return DXGI_FORMAT_UNKNOWN;
	}
}


static DXGI_FORMAT GetStencilSRVFormat(DXGI_FORMAT depthFormat)
{
	switch (depthFormat)
	{
		case DXGI_FORMAT_D16_UNORM:
			return DXGI_FORMAT_UNKNOWN;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
		case DXGI_FORMAT_D32_FLOAT:
			return DXGI_FORMAT_UNKNOWN;
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
		default:
			return DXGI_FORMAT_UNKNOWN;
	}
}


static bool IsDepth(DXGI_FORMAT format)
{
	return format == DXGI_FORMAT_D16_UNORM || format == DXGI_FORMAT_D24_UNORM_S8_UINT || format == DXGI_FORMAT_D32_FLOAT ||
	       format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
}


bool RenderTarget::Init(Device* device, DXGI_FORMAT format, uint32_t width, uint32_t height, const wchar_t* name, ERenderTargetFlags flags)
{
	Release(device);

	bool isDepth = IsDepth(format);
	bool isCubemap = flags & kRenderTargetCubemap;
	bool withMips = flags & kRenderTargetWithMips;
	bool allowRtvDsv = flags & kRenderTargetAllowRTV_DSV;
	bool allowUav = flags & kRenderTargetAllowUAV;

	m_width = width;
	m_height = height;
	m_arraySize = isCubemap ? 6 : 1;
	m_mipLevels = withMips ? ComputeMipLevelsNum(width, height) : 1;
	m_format = format;
	m_flags = flags;

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = m_arraySize;
	texDesc.MipLevels = m_mipLevels;
	texDesc.Format = format;
	texDesc.SampleDesc = {1, 0};
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	if (allowRtvDsv)
		texDesc.Flags |= isDepth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (allowUav)
		texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clearValue;
	clearValue.Format = format;

	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
	if (isDepth)
	{
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;
	}
	else
	{
		memset(clearValue.Color, 0, sizeof(clearValue.Color));
	}
	D3D12_CLEAR_VALUE* clearValuePtr = allowRtvDsv ? &clearValue : nullptr;

	HRESULT hr = device->GetDevice()->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &texDesc, initialState, clearValuePtr, IID_PPV_ARGS(&texture));
	if (FAILED(hr))
		return false;

	texture->SetName(name);

	if (allowRtvDsv)
	{
		if (isDepth)
			dsv = device->CreateDSV(texture, nullptr);
		else
			rtv = device->CreateRTV(texture, nullptr);
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = isDepth ? GetDepthSRVFormat(format) : format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if (isCubemap)
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = texDesc.MipLevels;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	srv = device->CreateSRV(texture, &srvDesc);

	uint32_t subresourcesNum = m_mipLevels * m_arraySize;
	subresourcesState.resize(subresourcesNum);
	for (D3D12_RESOURCE_STATES& state : subresourcesState)
		state = initialState;

	return true;
}


void RenderTarget::Release(Device* device)
{
	if (IsDepth(m_format))
	{
		device->DestroyDSV(dsv);
		dsv.Invalidate();
	}
	else
	{
		device->DestroyRTV(rtv);
		rtv.Invalidate();
	}
	device->DestroySRV(srv);
	srv.Invalidate();
	SafeRelease(texture);
	subresourcesState.clear();
}


bool RenderTarget::TransitionTo(D3D12_RESOURCE_STATES newState,
                                std::vector<D3D12_RESOURCE_BARRIER>& barriers,
                                uint32_t subresource)
{
	if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		uint32_t counter = 0;
		for (size_t i = 0; i < subresourcesState.size(); i++)
		{
			D3D12_RESOURCE_STATES& state = subresourcesState[i];
			if (state == newState)
				continue;

			barriers.push_back(TransitionBarrier(texture, state, newState, i));
			state = newState;
			counter++;
		}

		return counter != 0;
	}
	
	D3D12_RESOURCE_STATES& state = subresourcesState[subresource];
	if (state == newState)
		return false;

	barriers.push_back(TransitionBarrier(texture, state, newState, subresource));
	state = newState;
	return true;
}
