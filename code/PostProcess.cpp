#include "Precompiled.h"
#include "PostProcess.h"

struct Params
{
	float EV100;
	uint32_t EnableTonemap;
	uint32_t DirectLightTextureIdx;
	uint32_t IndirectLightTextureIdx;
};


bool PostProcess::Init(Device* device)
{
	PIXScopedEvent(0, "PostProcess::Init");
	m_device = device;
	ReloadShaders();
	return true;
}


void PostProcess::Shutdown()
{
	SafeRelease(m_pso);
}


void PostProcess::Render(ID3D12GraphicsCommandList* cmdList, SRVHandle directLightSRV, SRVHandle indirectLightSRV)
{
	if (!m_pso)
		return;

	PIXScopedEvent(cmdList, 0, "PostProcess");

	Params params;
	params.EV100 = ev100;
	params.EnableTonemap = enableTonemap ? 1 : 0;
	params.DirectLightTextureIdx = directLightSRV.idx;
	params.IndirectLightTextureIdx = indirectLightSRV.idx;
 	D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddr = m_device->UpdateConstantBuffer(&params, sizeof(params));

	cmdList->SetGraphicsRootConstantBufferView(1, cbGpuAddr);
	cmdList->SetPipelineState(m_pso);
	cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);
}


bool PostProcess::ReloadShaders()
{
	m_device->DeletePSO(m_pso);

	GfxPipelineStateDesc psoDesc;
	psoDesc.name = L"PostProcess";
	psoDesc.vs = L"quad.vs";
	psoDesc.ps = L"postprocess.ps";
	psoDesc.RasterizerState = GetDefaultRasterizerDesc();
	psoDesc.BlendState = GetDefaultBlendDesc();
	psoDesc.DepthStencilState = GetDefaultDepthStencilDesc();
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
	m_pso = m_device->CreatePSO(psoDesc);

	return m_pso != nullptr;
}
