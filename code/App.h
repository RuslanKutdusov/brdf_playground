#pragma once
#include "Device.h"
#include "Window.h"
#include "ImguiWrap.h"
#include "Camera.h"
#include "RenderTarget.h"
#include "EnvEmitter.h"
#include "EnvMapFilter.h"
#include "BrdfRenderer.h"
#include "ObjRenderer.h"
#include "PostProcess.h"
#include "SpectralPowerDistribution.h"
#include "Fresnel.h"


__declspec(align(16)) struct GlobalConstBuffer
{
	uint32_t PrefilteredDiffuseEnvMap;
	uint32_t BRDFLut;
	uint32_t PrefilteredSpecularEnvMap;
	uint32_t ShadowMap;
	uint32_t EnvironmentMap;
	uint32_t pad0;
	uint32_t pad1;
	uint32_t pad2;
	DirectX::XMMATRIX ViewProjMatrix;
	DirectX::XMVECTOR ViewPos;
	DirectX::XMVECTOR LightDir;
	DirectX::XMVECTOR LightIlluminance;
	DirectX::XMMATRIX ShadowViewProjMatrix;
	DirectX::XMMATRIX ShadowMatrix;
	uint32_t SamplingType;
	uint32_t FrameIdx;
	uint32_t TotalSamples;
	uint32_t SamplesPerFrame;
	uint32_t SamplesProcessed;
	uint32_t EnableDirectLight;
	uint32_t EnableEnvEmitter;
	uint32_t EnableShadow;
	uint32_t EnableDiffuseBRDF;
	uint32_t EnableSpecularBRDF;
	uint32_t ScreenWidth;
	uint32_t ScreenHeight;
};


enum ESceneType
{
	kSceneSingleObject = 0,
	kSceneObjectsGrid,
	kSceneBrdfLobe,

	kSceneTypesCount
};


enum EObjectType
{
	kObjectSphere = 0,
	kObjectCube,
	kObjectShaderBall,
	kObjectTypesCount
};


enum EMaterialType
{
	kMaterialSimple = 0,
	kMaterialSmoothDiffuse,
	kMaterialRoughDiffuse,
	kMaterialSmoothConductor,
	kMaterialRoughConductor,
	kMaterialRoughPlastic,
	kMaterialTexture,
	kMaterialMERL,
	kMaterialTypesCount
};


enum ESamplingType
{
	kSamplingTypeIS = 0,
	kSamplingTypeFIS,
	kSamplingTypeSplitSum,
	kSamplingTypeSplitSumNV,
	kSamplingTypeBakedSplitSumNV,
	kSamplingTypesCount
};


enum EEnvEmitterType
{
	kEnvEmitterTexture = 0,
	kEnvEmitterConst
};


struct SingleObjectSceneControls
{
	EObjectType objType = kObjectSphere;
	const char* textureMaterial = "";
	const char* merlMaterial = "";
	const char* ior = "";
};


struct ObjectsGridSceneControls
{
	EObjectType objType = kObjectSphere;
};


class PlotsWindow
{
public:
	void Init(const std::vector<FilePath>& spdFiles);
	void Show();
	void Update();

private:
	enum EPlotType
	{
		kFresnelRGBPlot = 0,
		kFresnelSpectralPlot,
		kIORPlot,
		kCIEPlot,
		kSpectrumPlot,
		kPlotTypesCount
	};

	std::vector<FilePath> m_spdFiles;
	EPlotType m_plotType = kFresnelRGBPlot;
	bool m_opened = false;
	const char* selectorIOR = "";

	Spectrum m_etaSpectrum;
	float m_etaSpectrumMaxVal = 0.0f;
	Spectrum m_kSpectrum;
	float m_kSpectrumMaxVal = 0.0f;
	std::vector<XMVECTOR> m_fresnelRGBPlot;
	bool m_drawFresnelRGBPlot = true;
	bool m_drawSchlickPlot = true;
	bool m_fresnelDrawRGB[3] = {true, true, true};
	float m_fresnelSpectralPlotLambda = kSpectrumMinWavelength + kSpectrumRange * 0.5f;
	std::vector<float> m_fresnelSpectralPlot;
	Spectrum m_customRGBSpectrum;
	Spectrum::ESpectrumType m_customRGBSpectrumType = Spectrum::kReflectance;
	XMFLOAT3 m_customRGB = {0.5f, 0.5f, 0.5f};
	XMFLOAT2 m_cieMinMax = {0.0f, -FLT_MAX};
	XMFLOAT2 m_rgbSpectrumsMinMax = {0.0f, -FLT_MAX};
	XMFLOAT2 m_d65MinMax = {0.0f, -FLT_MAX};
	XMFLOAT2 m_d65NormalizedMinMax = {0.0f, -FLT_MAX};
	int m_spectrumPlotType = 0;

	void BuildFresnelRGBPlot(uint32_t pointsNum);
	void DrawFresnelRGBPlot();
	void BuildFresnelSpectralPlot(uint32_t pointsNum);
	void DrawFresnelSpectralPlot();
	void DrawIORPlot();
	void DrawCIEPlot();
	void DrawSpectrumPlot();
	void LoadSPDs(const char* path);
};


class App
{
public:
	App();
	~App();

	bool Init();
	void Run();

private:
	Device m_device;
	Window m_window;
	ImguiWrap m_imguiWrap;
	PlotsWindow m_plotsWindow;

	FirstPersonCamera m_firstPersonCamera;
	OrbitCamera m_orbitCamera;

	bool m_vsync = true;

	GlobalConstBuffer m_globalConstBuffer;
	RenderTarget m_directLightRT;
	RenderTarget m_indirectLightRT;
	RenderTarget m_depthTarget;
	RenderTarget m_shadowMap;

	EnvEmitter m_envEmitter;
	EnvMapFilter m_envMapFilter;
	ObjRenderer m_objRenderer;
	BrdfRenderer m_brdfRenderer;
	PostProcess m_postProcess;

	// ui stuff
	bool m_enableDirectLight = true;
	bool m_enableShadow = true;
	float m_lightDirVert = 45.0f;
	float m_lightDirHor = 130.0f;
	float m_lightIlluminance = 1.0f;
	DirectX::XMVECTOR m_lightColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);

	bool m_enableEnvEmitter = true;

	uint32_t m_samplesCount = 128;
	uint32_t m_samplesPerFrame = 16;
	ESamplingType m_samplingType = kSamplingTypeFIS;
	DirectX::XMMATRIX m_prevFrameViewProj;
	bool m_resetSampling = false;

	bool m_enableDiffuseBRDF = true;
	bool m_enableSpecularBRDF = true;

	ESceneType m_sceneType = kSceneSingleObject;
	SingleObjectSceneControls m_singleObjScene;
	ObjRenderer::InstanceData m_singleObjInstanceData;
	ObjectsGridSceneControls m_objsGridScene;
	std::vector<ObjRenderer::InstanceData> m_objsGridInstancesData;

	Model m_models[kObjectTypesCount];
	Material m_material;
	std::vector<FilePath> m_hdrFiles;
	std::vector<FilePath> m_materials;
	std::vector<FilePath> m_spdFiles;
	std::vector<FilePath> m_merlMaterials;

	void InitUI();
	void OnNewFrame();
	void OnRender();
	void UpdateUI();
	void UpdateGlobalConstBuffer(const DirectX::XMMATRIX& viewProj);
	PerspectiveCamera* GetCurrentCamera();
	void RenderScene(ID3D12GraphicsCommandList* cmdList);
	void ExportToMitsuba();
	XMVECTOR ComputeF0(const char* ior);
};
