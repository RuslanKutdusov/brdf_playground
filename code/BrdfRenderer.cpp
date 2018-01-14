#include "Precompiled.h"
#include "BrdfRenderer.h"

struct LobeConstBuffer
{
	float roughness;
	float metalness;
	float reflectance;
	float illuminance;
	DirectX::XMVECTOR color;
};


struct LineConstBuffer
{
	DirectX::XMVECTOR start;
	DirectX::XMVECTOR end;
	DirectX::XMVECTOR color;
};


bool BrdfRenderer::Init(Device* device)
{
	PIXScopedEvent(0, "BrdfRenderer::Init");
	m_device = device;
	
	ReloadShaders();

	HRESULT hr = m_hemisphereModel.Load(m_device, "models\\hemisphere.obj", false, TextureMapping());
	if (FAILED(hr))
		return false;

	return true;
}


void BrdfRenderer::Shutdown()
{
	m_device->DeletePSO(m_lobePSO);
	m_device->DeletePSO(m_linePSO);
	m_hemisphereModel.Release(m_device);
}


bool BrdfRenderer::ReloadShaders()
{
	const D3D12_INPUT_ELEMENT_DESC elementsDesc[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	GfxPipelineStateDesc psoDesc;
	psoDesc.name = L"BRDFLobe";
	psoDesc.InputLayout = {elementsDesc, _countof(elementsDesc)};
	psoDesc.vs = L"brdf_lobe.hlsl";
	psoDesc.ps = L"brdf_lobe.hlsl";
	psoDesc.RasterizerState = GetDefaultRasterizerDesc();
	psoDesc.BlendState = GetDefaultBlendDesc();
	psoDesc.DepthStencilState = GetDefaultDepthStencilDesc();
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	m_lobePSO = m_device->CreatePSO(psoDesc);

	psoDesc.name = L"Line";
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	psoDesc.vs = L"line.hlsl";
	psoDesc.ps = L"line.hlsl";
	m_linePSO = m_device->CreatePSO(psoDesc);

	return m_lobePSO && m_linePSO;
}


void BrdfRenderer::Render(ID3D12GraphicsCommandList* cmdList, const DirectX::XMVECTOR& lightVector)
{
	if (!m_lobePSO || !m_linePSO)
		return;

	PIXScopedEvent(cmdList, 0, "BRDF Lobe");

	// lines
	RenderLine(DirectX::XMVectorScale(lightVector, 100.0f), DirectX::XMVectorSet(0.2f, 1.0f, 0.2f, 0.0f), cmdList);

	DirectX::XMVECTOR axis = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	RenderLine(axis, axis, cmdList);

	axis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	RenderLine(axis, axis, cmdList);

	axis = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	RenderLine(axis, axis, cmdList);

	// lobe
	LobeConstBuffer constBuf;
	constBuf.roughness = roughness;
	constBuf.metalness = metalness;
	constBuf.reflectance = reflectance;
	constBuf.illuminance = illuminance;
	constBuf.color = color;

	D3D12_GPU_VIRTUAL_ADDRESS gpuCbAddr = m_device->UpdateConstantBuffer(&constBuf, sizeof(constBuf));
	cmdList->SetGraphicsRootConstantBufferView(1, gpuCbAddr);
	cmdList->SetPipelineState(m_lobePSO);
	Mesh& mesh = m_hemisphereModel.meshes[0];
	cmdList->IASetVertexBuffers(0, 1, &m_hemisphereModel.vbv);
	cmdList->IASetIndexBuffer(&m_hemisphereModel.ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
}


void BrdfRenderer::RenderLine(const DirectX::XMVECTOR& end, const DirectX::XMVECTOR& color, ID3D12GraphicsCommandList* cmdList)
{
	LineConstBuffer lineConstBuf;
	lineConstBuf.start = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	lineConstBuf.end = end;
	lineConstBuf.color = color;
	D3D12_GPU_VIRTUAL_ADDRESS gpuCbAddr = m_device->UpdateConstantBuffer(&lineConstBuf, sizeof(lineConstBuf));
	cmdList->SetGraphicsRootConstantBufferView(1, gpuCbAddr);
	cmdList->SetPipelineState(m_linePSO);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	cmdList->DrawInstanced(2, 1, 0, 0);
}
