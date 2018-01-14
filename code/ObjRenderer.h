#pragma once

class ObjRenderer
{
public:
	struct InstanceData
	{
		DirectX::XMMATRIX WorldMatrix;
		float Metalness;
		float Roughness;
		float Reflectance;
		float padding0[1];
		DirectX::XMVECTOR BaseColor;
		uint32_t MaterialType;
		uint32_t padding1[3];
	};
	const uint32_t MAX_INSTANCES = 128;

	bool Init(Device* device);
	void Shutdown();

	bool ReloadShaders();

	void RenderShadowPass(ID3D12GraphicsCommandList* cmdList, const Model* model, const InstanceData* instancesData, uint32_t instancesNum);
	void RenderDepthPass(ID3D12GraphicsCommandList* cmdList, const Model* model, const InstanceData* instancesData, uint32_t instancesNum);
	void RenderLightPass(ID3D12GraphicsCommandList* cmdList,
	                     const Model* model,
	                     const InstanceData* instancesData,
	                     uint32_t instancesNum,
	                     Material* material /*, MERLMaterial* merlMaterial */);

private:
	Device* m_device = nullptr;
	ID3D12PipelineState* m_shadowPso = nullptr;
	ID3D12PipelineState* m_depthPso = nullptr;
	ID3D12PipelineState* m_lightPso = nullptr;

	void Render(ID3D12GraphicsCommandList* cmdList, const Model* model, const InstanceData* instancesData, uint32_t instancesNum);
};
