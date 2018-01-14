#pragma once

class PostProcess
{
public:
	bool Init(Device* device);
	void Shutdown();

	void Render(ID3D12GraphicsCommandList* cmdList,
	            SRVHandle directLightSRV,
	            SRVHandle indirectLightSRV);
	bool ReloadShaders();

	float ev100 = 0.0f;
	bool enableTonemap = true;

private:
	Device* m_device = nullptr;
	ID3D12PipelineState* m_pso = nullptr;
};