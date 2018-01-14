#pragma once
#include "Model.h"
#include "RenderTarget.h"

class EnvEmitter
{
public:
	enum EType
	{
		kTypeTexture = 0,
		kTypeConstLuminance,
	};

	bool Init(Device* device);
	void Shutdown();

	void BakeCubemap(ID3D12GraphicsCommandList* cmdList);
	void RenderDepthPass(ID3D12GraphicsCommandList* cmdList);
	void RenderLightPass(ID3D12GraphicsCommandList* cmdList);

	bool ReloadShaders();

	void SetScale(float scale);
	void SetType(EType type);
	void SetConstLuminanceColor(const DirectX::XMVECTOR& constLuminanceColor);
	void SetConstLuminance(float constLuminance);
	void SetTexture(const char* fileName);

	float GetScale() const;
	EType GetType() const;
	const DirectX::XMVECTOR& GetConstLuminanceColor();
	float GetConstLuminance();
	const char* GetTextureFileName() const;
	SRVHandle GetCubeMapSRV() const;

private:
	struct ConstBuffer
	{
		DirectX::XMMATRIX BakeViewProj[6];
		DirectX::XMVECTOR ConstLuminanceColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
		float ConstLuminance = 1.0f;
		uint32_t UseConstLuminance = 1;
		float Scale = 1.0f;
		uint32_t UseCubeTexture = 0;
		uint32_t CubeTextureIdx;
		uint32_t TextureIdx;
	};

	Device* m_device = nullptr;
	ID3D12PipelineState* m_depthPSO = nullptr;
	ID3D12PipelineState* m_lightPSO = nullptr;
	ID3D12PipelineState* m_cubemapBakePSO = nullptr;
	ID3D12PipelineState* m_mipsGenPSO = nullptr;
	Model m_sphereModel;
	RenderTarget m_cubemap;
	SRVHandle m_cubeMap2DArraySRV;
	std::vector<UAVHandle> m_cubemapUAVs; // for each mip
	ID3D12Resource* m_dummyTexture = nullptr;
	SRVHandle m_dummyCubeSRV;
	SRVHandle m_dummy2DSRV;
	FilePath m_textureFileName;
	ID3D12Resource* m_texture = nullptr;
	SRVHandle m_textureSRV;
	ConstBuffer m_constBufferData;

	void Draw(ID3D12GraphicsCommandList* cmdList, uint32_t numInstances);
	void UpdateAndBindConstBuffer(ID3D12GraphicsCommandList* cmdList);
};


inline void EnvEmitter::SetScale(float scale)
{
	m_constBufferData.Scale = scale;
}


inline void EnvEmitter::SetType(EType type)
{
	m_constBufferData.UseConstLuminance = type == kTypeConstLuminance ? 1 : 0;
}


inline void EnvEmitter::SetConstLuminanceColor(const DirectX::XMVECTOR& constLuminanceColor)
{
	m_constBufferData.ConstLuminanceColor = constLuminanceColor;
}


inline void EnvEmitter::SetConstLuminance(float constLuminance)
{
	m_constBufferData.ConstLuminance = constLuminance;
}


inline float EnvEmitter::GetScale() const
{
	return m_constBufferData.Scale;
}


inline EnvEmitter::EType EnvEmitter::GetType() const
{
	return m_constBufferData.UseConstLuminance ? kTypeConstLuminance : kTypeTexture;
}


inline const DirectX::XMVECTOR& EnvEmitter::GetConstLuminanceColor()
{
	return m_constBufferData.ConstLuminanceColor;
}


inline float EnvEmitter::GetConstLuminance()
{
	return m_constBufferData.ConstLuminance;
}


inline const char* EnvEmitter::GetTextureFileName() const
{
	return m_textureFileName.c_str();
}


inline SRVHandle EnvEmitter::GetCubeMapSRV() const
{
	return m_cubemap.srv;
}