#pragma once

class EnvMapFilter
{
public:
	bool Init(Device* device);
	void Shutdown();

	bool ReloadShaders();

	void FilterEnvMap(ID3D12GraphicsCommandList* cmdList, SRVHandle envmap);

	SRVHandle GetPrefilteredSpecEnvMap();
	SRVHandle GetBRDFLut();
	SRVHandle GetPrefilteredDiffEnvMap();

private:
	Device* m_device = nullptr;

	ID3D12PipelineState* m_envMapSpecPrefilter = nullptr;
	ID3D12PipelineState* m_brdfLutGen = nullptr;
	ID3D12PipelineState* m_envMapDiffPrefilter = nullptr;

	RenderTarget m_prefilteredSpecEnvMap;
	std::vector<UAVHandle> m_prefilteredSpecEnvMapUAVs;  // for each mip

	RenderTarget m_brdfLut;
	UAVHandle m_brdfLutUav;

	RenderTarget m_prefilteredDiffEnvMap;
	UAVHandle m_prefilteredDiffEnvMapUAV;
};


inline SRVHandle EnvMapFilter::GetPrefilteredSpecEnvMap()
{
	return m_prefilteredSpecEnvMap.srv;
}


inline SRVHandle EnvMapFilter::GetBRDFLut()
{
	return m_brdfLut.srv;
}


inline SRVHandle EnvMapFilter::GetPrefilteredDiffEnvMap()
{
	return m_prefilteredDiffEnvMap.srv;
}
