#pragma once
#include "Model.h"

class BrdfRenderer
{
public:
	bool Init(Device* device);
	void Shutdown();
	bool ReloadShaders();
	void Render(ID3D12GraphicsCommandList* cmdList, const DirectX::XMVECTOR& lightVector);

	float roughness = 1.0f;
	float metalness = 0.5f;
	float reflectance = 1.0f;
	float illuminance = 1.0f;
	DirectX::XMVECTOR color = DirectX::XMVectorSet(0.2f, 0.0f, 0.0f, 0.0f);

private:
	Device* m_device = nullptr;
	Model m_hemisphereModel;
	ID3D12PipelineState* m_lobePSO;
	ID3D12PipelineState* m_linePSO;

	void RenderLine(const DirectX::XMVECTOR& end, const DirectX::XMVECTOR& color, ID3D12GraphicsCommandList* cmdList);
};
