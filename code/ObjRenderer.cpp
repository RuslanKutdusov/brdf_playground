#include "Precompiled.h"
#include "Model.h"
#include "MERLMaterial.h"
#include "ObjRenderer.h"

struct MaterialConstBuf
{
	uint32_t BaseColorTexture;
	uint32_t NormalTexture;
	uint32_t RoughnessTexture;
	uint32_t MetalnessTexture;
	uint32_t AoTexture;
	uint32_t MerlBRDF;
};


bool ObjRenderer::Init(Device* device)
{
	PIXScopedEvent(0, "ObjRender::Init");
	m_device = device;
	ReloadShaders();
	return true;
}


void ObjRenderer::Shutdown()
{
	m_device->DeletePSO(m_shadowPso);
	m_device->DeletePSO(m_depthPso);
	m_device->DeletePSO(m_lightPso);
}


bool ObjRenderer::ReloadShaders()
{
	m_device->DeletePSO(m_shadowPso);
	m_device->DeletePSO(m_depthPso);
	m_device->DeletePSO(m_lightPso);

	GfxPipelineStateDesc psoDesc;
	psoDesc.name = L"ObjDepth";
	psoDesc.InputLayout = {kMeshElementsDesc, _countof(kMeshElementsDesc)};
	psoDesc.vs = L"object.hlsl";
	psoDesc.RasterizerState = GetDefaultRasterizerDesc();
	psoDesc.BlendState = GetDefaultBlendDesc();
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
	psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = 0;
	psoDesc.DepthStencilState = GetDefaultDepthStencilDesc();
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	m_depthPso = m_device->CreatePSO(psoDesc);

	psoDesc.name = L"ObjShadow";
	psoDesc.defines.push_back(L"SHADOW_PASS");
	psoDesc.DSVFormat = DXGI_FORMAT_D16_UNORM;
	m_shadowPso = m_device->CreatePSO(psoDesc);

	psoDesc.name = L"ObjLight";
	psoDesc.defines.clear();
	psoDesc.ps = L"object.hlsl";
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
	psoDesc.NumRenderTargets = 2;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;
	psoDesc.BlendState.IndependentBlendEnable = true;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.BlendState.RenderTarget[1].BlendEnable = true;
	psoDesc.BlendState.RenderTarget[1].SrcBlend = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[1].DestBlend = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[1].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[1].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[1].DestBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[1].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = 0x0f;
	psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	m_lightPso = m_device->CreatePSO(psoDesc);

	return m_depthPso && m_lightPso;
}


void ObjRenderer::Render(ID3D12GraphicsCommandList* cmdList, const Model* model, const InstanceData* instancesData, uint32_t instancesNum)
{
	D3D12_GPU_VIRTUAL_ADDRESS gpuCbAddr = m_device->UpdateConstantBuffer(instancesData, sizeof(InstanceData) * instancesNum);
	cmdList->SetGraphicsRootConstantBufferView(1, gpuCbAddr);
	cmdList->IASetVertexBuffers(0, 1, &model->vbv);
	cmdList->IASetIndexBuffer(&model->ibv);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (const Mesh& mesh : model->meshes)
		cmdList->DrawIndexedInstanced(mesh.indexCount, instancesNum, mesh.firstIndex, mesh.firstVertex, 0);
}


void ObjRenderer::RenderShadowPass(ID3D12GraphicsCommandList* cmdList, const Model* model, const InstanceData* instancesData, uint32_t instancesNum)
{
	if (!m_shadowPso)
		return;

	cmdList->SetPipelineState(m_shadowPso);
	Render(cmdList, model, instancesData, instancesNum);
}


void ObjRenderer::RenderDepthPass(ID3D12GraphicsCommandList* cmdList, const Model* model, const InstanceData* instancesData, uint32_t instancesNum)
{
	if (!m_depthPso)
		return;

	cmdList->SetPipelineState(m_depthPso);
	Render(cmdList, model, instancesData, instancesNum);
}


void ObjRenderer::RenderLightPass(ID3D12GraphicsCommandList* cmdList,
                                  const Model* model,
                                  const InstanceData* instancesData,
                                  uint32_t instancesNum,
                                  Material* material /*, MERLMaterial* merlMaterial */)
{
	if (!m_lightPso)
		return;

	MaterialConstBuf constBuf = {};
	if (material)
	{
		constBuf.BaseColorTexture = material->GetTexture(Material::kBaseColor).idx;
		constBuf.NormalTexture = material->GetTexture(Material::kNormal).idx;
		constBuf.RoughnessTexture = material->GetTexture(Material::kRoughness).idx;
		constBuf.MetalnessTexture = material->GetTexture(Material::kMetalness).idx;
		constBuf.AoTexture = material->GetTexture(Material::kAO).idx;
	}
	/*if (merlMaterial)
	    srv[4] = merlMaterial->m_brdfSRV;*/

	D3D12_GPU_VIRTUAL_ADDRESS gpuCbAddr = m_device->UpdateConstantBuffer(&constBuf, sizeof(constBuf));
	cmdList->SetGraphicsRootConstantBufferView(2, gpuCbAddr);

	cmdList->SetPipelineState(m_lightPso);
	Render(cmdList, model, instancesData, instancesNum);
}