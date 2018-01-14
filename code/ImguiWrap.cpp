#include "Precompiled.h"
#include "Window.h"
#include "ImguiWrap.h"
#include "imgui\imgui.h"
#include "imgui\imgui_impl_win32.h"


struct ConstantBuffer
{
	float mvp[4][4];
	uint32_t textureIndex;
};


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


bool ImguiWrap::FrameResources::Prepare(ID3D12Device5* d3dDevice, ImDrawData* draw_data)
{
	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = S_OK;

	if (!vertexBuffer || vertexBufferSize < draw_data->TotalVtxCount)
	{
		SafeRelease(vertexBuffer);
		vertexBufferSize = draw_data->TotalVtxCount + 5000;
		bufferDesc.Width = vertexBufferSize * sizeof(ImDrawVert);
		hr = d3dDevice->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
		                                        IID_PPV_ARGS(&vertexBuffer));
		if (FAILED(hr))
		{
			LogStdErr("ID3D12Device::CreateCommittedResource failed: %x\n", hr);
			return false;
		}
	}

	if (indexBuffer == NULL || indexBufferSize < draw_data->TotalIdxCount)
	{
		SafeRelease(indexBuffer);
		indexBufferSize = draw_data->TotalIdxCount + 10000;
		bufferDesc.Width = indexBufferSize * sizeof(ImDrawIdx);
		hr = d3dDevice->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
		                                        IID_PPV_ARGS(&indexBuffer));
		if (FAILED(hr))
		{
			LogStdErr("ID3D12Device::CreateCommittedResource failed: %x\n", hr);
			return false;
		}
	}

	return true;
}


void ImguiWrap::FrameResources::Shutdown()
{
	SafeRelease(vertexBuffer);
	SafeRelease(indexBuffer);
}


bool ImguiWrap::Init(Window& window, Device& device)
{
	PIXScopedEvent(0, "ImguiWrap::Init");
	m_window = &window;
	m_device = &device;

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	/*RECT windowRect = window.GetClientRect();
	io.DisplaySize.x = windowRect.right;
	io.DisplaySize.y = windowRect.bottom;*/
	io.BackendRendererName = "dx12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
	io.FontGlobalScale = (float)GetDpiForWindow(window.GetHWND()) / 100.0f;
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window.GetHWND());

	if (!CreatePSO())
		return false;

	if (!CreateFontsTextures())
		return false;

	return true;
}


void ImguiWrap::Shutdown()
{
	for (auto& fr : m_frameResources)
		fr.Shutdown();
	m_device->DestroySRV(m_fontsTextureSRV);
	SafeRelease(m_fonstTexture);
	m_device->DeletePSO(m_pso);
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}


void ImguiWrap::OnNewFrame()
{
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}


void ImguiWrap::RenderDrawData()
{
	ImGui::Render();

	ImDrawData* draw_data = ImGui::GetDrawData();

	// Avoid rendering when minimized
	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
		return;

	FrameResources& fr = m_frameResources[m_device->GetFrameIdx()];
	if (!fr.Prepare(m_device->GetDevice(), draw_data))
		return;

	// Upload vertex/index data into a single contiguous GPU buffer
	void *vtx_resource, *idx_resource;
	D3D12_RANGE range;
	memset(&range, 0, sizeof(D3D12_RANGE));
	if (fr.vertexBuffer->Map(0, &range, &vtx_resource) != S_OK)
		return;
	if (fr.indexBuffer->Map(0, &range, &idx_resource) != S_OK)
		return;
	ImDrawVert* vtx_dst = (ImDrawVert*)vtx_resource;
	ImDrawIdx* idx_dst = (ImDrawIdx*)idx_resource;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}
	fr.vertexBuffer->Unmap(0, &range);
	fr.indexBuffer->Unmap(0, &range);

	// Setup desired DX state
	SetupRenderState(draw_data);

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	ID3D12GraphicsCommandList* cmdList = m_device->GetCommandList();
	int global_vtx_offset = 0;
	int global_idx_offset = 0;
	ImVec2 clip_off = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != NULL)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
					SetupRenderState(draw_data);
				else
					pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				// Apply Scissor, Bind texture, Draw
				const D3D12_RECT r = {(LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x),
				                      (LONG)(pcmd->ClipRect.w - clip_off.y)};
				cmdList->RSSetScissorRects(1, &r);
				cmdList->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
}


bool ImguiWrap::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);
}


bool ImguiWrap::WantMouseAndKeyboard() const
{
	if (ImGui::GetCurrentContext())
	{
		ImGuiIO& io = ImGui::GetIO();
		return io.WantCaptureMouse || io.WantCaptureKeyboard;
	}
	return false;
}


bool ImguiWrap::CreatePSO()
{
	GfxPipelineStateDesc psoDesc = {};
	psoDesc.name = L"ImGui";
	psoDesc.vs = L"imgui.hlsl";
	psoDesc.ps = L"imgui.hlsl";
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = Window::kFormat;

	D3D12_INPUT_ELEMENT_DESC inputElements[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)IM_OFFSETOF(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)IM_OFFSETOF(ImDrawVert, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	    {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)IM_OFFSETOF(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	psoDesc.InputLayout = {inputElements, _countof(inputElements)};

	// Create the blending setup
	{
		D3D12_BLEND_DESC& desc = psoDesc.BlendState;
		desc.AlphaToCoverageEnable = false;
		desc.RenderTarget[0].BlendEnable = true;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	// Create the rasterizer state
	{
		D3D12_RASTERIZER_DESC& desc = psoDesc.RasterizerState;
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_NONE;
		desc.FrontCounterClockwise = FALSE;
		desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.DepthClipEnable = true;
		desc.MultisampleEnable = FALSE;
		desc.AntialiasedLineEnable = FALSE;
		desc.ForcedSampleCount = 0;
		desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}

	// Create depth-stencil State
	{
		D3D12_DEPTH_STENCIL_DESC& desc = psoDesc.DepthStencilState;
		desc.DepthEnable = false;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.StencilEnable = false;
		desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		desc.BackFace = desc.FrontFace;
	}

	m_pso = m_device->CreatePSO(psoDesc);
	return m_pso != nullptr;
}


bool ImguiWrap::CreateFontsTextures()
{
	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

	HRESULT hr = m_device->GetDevice()->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COMMON, NULL,
	                                                            IID_PPV_ARGS(&m_fonstTexture));
	if (FAILED(hr))
	{
		LogStdErr("ID3D12Device::CreateCommandAllocator failed: %x\n", hr);
		return false;
	}
	m_fonstTexture->SetName(L"ImguiFontsTexture");

	m_fontsTextureSRV = m_device->CreateSRV(m_fonstTexture, nullptr);
	io.Fonts->TexID = (ImTextureID)m_fontsTextureSRV.idx;

	m_device->BeginTransfer();
	m_device->UploadTextureSubresource(m_fonstTexture, 0, 0, pixels, width * 4);
	m_device->EndTransfer();

	return true;
}


void ImguiWrap::SetupRenderState(ImDrawData* draw_data)
{
	// Setup orthographic projection matrix into our constant buffer
	// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
	ConstantBuffer constBufData;
	{
		float L = draw_data->DisplayPos.x;
		float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		float T = draw_data->DisplayPos.y;
		float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
		float mvp[4][4] = {
		    {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
		    {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
		    {0.0f, 0.0f, 0.5f, 0.0f},
		    {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
		};
		memcpy(&constBufData.mvp, mvp, sizeof(mvp));
	}
	constBufData.textureIndex = m_fontsTextureSRV.idx;

	D3D12_GPU_VIRTUAL_ADDRESS constBuf = m_device->UpdateConstantBuffer(&constBufData, sizeof(constBufData));

	ID3D12GraphicsCommandList* cmdList = m_device->GetCommandList();
	FrameResources& fr = m_frameResources[m_device->GetFrameIdx()];

	// Setup viewport
	D3D12_VIEWPORT vp;
	memset(&vp, 0, sizeof(D3D12_VIEWPORT));
	vp.Width = draw_data->DisplaySize.x;
	vp.Height = draw_data->DisplaySize.y;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = vp.TopLeftY = 0.0f;
	cmdList->RSSetViewports(1, &vp);

	// Bind shader and vertex buffers
	unsigned int stride = sizeof(ImDrawVert);
	unsigned int offset = 0;
	D3D12_VERTEX_BUFFER_VIEW vbv;
	memset(&vbv, 0, sizeof(D3D12_VERTEX_BUFFER_VIEW));
	vbv.BufferLocation = fr.vertexBuffer->GetGPUVirtualAddress() + offset;
	vbv.SizeInBytes = fr.vertexBufferSize * stride;
	vbv.StrideInBytes = stride;
	cmdList->IASetVertexBuffers(0, 1, &vbv);

	D3D12_INDEX_BUFFER_VIEW ibv;
	memset(&ibv, 0, sizeof(D3D12_INDEX_BUFFER_VIEW));
	ibv.BufferLocation = fr.indexBuffer->GetGPUVirtualAddress();
	ibv.SizeInBytes = fr.indexBufferSize * sizeof(ImDrawIdx);
	ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	cmdList->IASetIndexBuffer(&ibv);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->SetPipelineState(m_pso);
	cmdList->SetGraphicsRootConstantBufferView(1, constBuf);

	// Setup blend factor
	const float blend_factor[4] = {0.f, 0.f, 0.f, 0.f};
	cmdList->OMSetBlendFactor(blend_factor);
}