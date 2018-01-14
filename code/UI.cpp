#include "Precompiled.h"
#include "App.h"
#include "ImguiWrap.h"
#include "imgui\imgui.h"
#include "imgui\imgui_impl_win32.h"
#include "Time.h"


static bool ColorDialog(XMVECTOR& color)
{
	CHOOSECOLOR cc;
	COLORREF acrCustClr[16];
	memset(&cc, 0, sizeof(cc));
	cc.lStructSize = sizeof(cc);
	cc.lpCustColors = (LPDWORD)acrCustClr;
	cc.rgbResult = LinearToPackedSRGB(color);
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;

	if (ChooseColor(&cc) == TRUE)
	{
		color = PackedSRGBToLinear(cc.rgbResult);
		return true;
	}

	return false;
}


static void EnumerateFiles(const char* searchDir, std::vector<FilePath>& list, bool dir = false)
{
	WIN32_FIND_DATAA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	hFind = FindFirstFileA(searchDir, &ffd);
	if (INVALID_HANDLE_VALUE == hFind)
		return;

	do
	{
		if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && !dir)
			continue;
		if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && dir)
			continue;
		if (ffd.cFileName[0] == '.' || strcmp(ffd.cFileName, "..") == 0)
			continue;
		list.push_back(std::move(ffd.cFileName));
	} while (FindNextFileA(hFind, &ffd) != 0);

	FindClose(hFind);
}


static bool SamplesCountCombo(const char* label, uint32_t& samplesCount, uint32_t maxSamples)
{
	const uint32_t samplesCountArr[] = {16, 32, 64, 128, 256, 512, 1024, 1536, 2048};
	const char* samplesCountStrArr[] = {"16", "32", "64", "128", "256", "512", "1024", "1536", "2048"};
	const uint32_t arrSize = _countof(samplesCountArr);
	uint32_t idx = 0;
	for (; idx < arrSize; idx++)
		if (samplesCountArr[idx] == samplesCount)
			break;

	uint32_t maxIdx = 0;
	for (; maxIdx < arrSize; maxIdx++)
		if (samplesCountArr[maxIdx] == maxSamples)
			break;

	bool retValue = false;
	if (ImGui::BeginCombo(label, samplesCountStrArr[idx]))
	{
		for (uint32_t n = 0; n <= maxIdx; n++)
		{
			bool is_selected = idx == n;
			if (ImGui::Selectable(samplesCountStrArr[n], is_selected))
			{
				samplesCount = samplesCountArr[n];
				retValue = true;
			}
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	return retValue;
}


template <typename LAMBDA>
static void Combo(const char* labal, const std::vector<FilePath>& files, const char* curValue, const LAMBDA& lambda)
{
	if (ImGui::BeginCombo(labal, curValue))
	{
		for (size_t n = 0; n < files.size(); n++)
		{
			bool is_selected = (files[n] == curValue);
			if (ImGui::Selectable(files[n].c_str(), is_selected))
				lambda(files[n].c_str(), n);
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}


void App::InitUI()
{
	EnumerateFiles("data\\HDRs\\*.dds", m_hdrFiles);
	EnumerateFiles("data\\HDRs\\*.hdr", m_hdrFiles);
	if (m_hdrFiles.empty())
	{
		m_envEmitter.SetType(EnvEmitter::kTypeConstLuminance);
	}
	else
	{
		std::sort(m_hdrFiles.begin(), m_hdrFiles.end());
		m_envEmitter.SetTexture(m_hdrFiles[0].c_str());
		m_envEmitter.SetType(EnvEmitter::kTypeTexture);
	}

	EnumerateFiles("data\\materials\\*", m_materials, true);
	if (!m_materials.empty())
		m_singleObjScene.textureMaterial = m_materials[0].c_str();

	EnumerateFiles("data\\MERL\\*", m_merlMaterials);
	if (!m_merlMaterials.empty())
		m_singleObjScene.merlMaterial = m_merlMaterials[0].c_str();
}


void App::UpdateUI()
{
	ImGui::Begin("Controls");
	if (ImGui::CollapsingHeader("Global controls", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Frame time: %.2f ms", Time::GetFrameDeltaTime() * 1000.0f);
		XMVECTOR p = GetCurrentCamera()->GetPosition();
		ImGui::Text("Camera position: %.2f %.2f %.2f", p.m128_f32[0], p.m128_f32[1], p.m128_f32[2]);
		ImGui::Text("Samples processed: %d", m_globalConstBuffer.SamplesProcessed);

		ImGui::Checkbox("VSync", &m_vsync);
		if (ImGui::Button("Reload shaders"))
		{
			m_envEmitter.ReloadShaders();
			m_envMapFilter.ReloadShaders();
			m_objRenderer.ReloadShaders();
			m_brdfRenderer.ReloadShaders();
			m_postProcess.ReloadShaders();
		}
	}

	if (ImGui::CollapsingHeader("Direct light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Enable light", &m_enableDirectLight);
		ImGui::SliderFloat("Vertical angle", &m_lightDirVert, 0.0f, 180.0f, "%.1f");
		ImGui::SliderFloat("Horizontal angle", &m_lightDirHor, 0.0f, 180.0f, "%.1f");
		ImGui::SliderFloat("Illuminance(lux)", &m_lightIlluminance, 0.0f, 20.0f, "%.1f");
		ImGui::Checkbox("Enable shadow", &m_enableShadow);
		if (ImGui::Button("Light color"))
			ColorDialog(m_lightColor);
	}

	if (ImGui::CollapsingHeader("Environment emitter", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Checkbox("Enable emitter", &m_enableEnvEmitter))
			m_resetSampling = true;

		EnvEmitter::EType type = m_envEmitter.GetType();
		if (ImGui::Combo("Emitter type", (int*)&type, "Texture\0Constant\0\0"))
		{
			m_envEmitter.SetType(type);
			m_resetSampling = true;
		}

		if (type == EnvEmitter::kTypeTexture)
		{
			Combo("HDR Texture", m_hdrFiles, m_envEmitter.GetTextureFileName(), [this](const char* selected, size_t idx) {
				m_envEmitter.SetTexture(selected);
				m_resetSampling = true;
			});

			float scale = m_envEmitter.GetScale();
			if (ImGui::SliderFloat("Scale", &scale, 0.0f, 5.0f, "%.1f"))
			{
				m_envEmitter.SetScale(scale);
				m_resetSampling = true;
			}
		}
		else
		{
			float constLum = m_envEmitter.GetConstLuminance();
			if (ImGui::SliderFloat("Luminance", &constLum, 0.0f, 5.0f, "%.1f"))
			{
				m_envEmitter.SetConstLuminance(constLum);
				m_resetSampling = true;
			}

			DirectX::XMVECTOR color = m_envEmitter.GetConstLuminanceColor();
			if (ImGui::Button("Emitter Color"))
			{
				if (ColorDialog(color))
				{
					m_envEmitter.SetConstLuminanceColor(color);
					m_resetSampling = true;
				}
			}
		}
	}

	if (ImGui::CollapsingHeader("Sampling", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (SamplesCountCombo("Samples count", m_samplesCount, 2048))
			m_resetSampling = true;
		if (SamplesCountCombo("Samples per frame", m_samplesPerFrame, m_samplesCount))
			m_resetSampling = true;

		if (m_samplesPerFrame > m_samplesCount)
		{
			m_samplesPerFrame = m_samplesCount;
			m_resetSampling = true;
		}

		const char* samplingTypes[] = {"Importance sampling", "Filtered importance sampling", "Split sum", "Split sum N=V", "Baked split sum"};
		if (ImGui::Combo("Sampling type", (int*)&m_samplingType, samplingTypes, kSamplingTypesCount))
			m_resetSampling = true;
	}

	if (ImGui::CollapsingHeader("BRDF", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Checkbox("Enable Diffuse BRDF", &m_enableDiffuseBRDF))
			m_resetSampling = true;
		if (ImGui::Checkbox("Enable Specular BRDF", &m_enableSpecularBRDF))
			m_resetSampling = true;
	}

	if (ImGui::CollapsingHeader("Film", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat("EV100", &m_postProcess.ev100, -5.0f, 25.0f, "%.1f");
		ImGui::Checkbox("Enable tonemap", &m_postProcess.enableTonemap);
	}

	if (ImGui::CollapsingHeader("Scene controls", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Combo("Scene", (int*)&m_sceneType, "Single object\0Objects grid\0BRDF\0Sponza\0\0"))
			m_resetSampling = true;

		if (m_sceneType == kSceneSingleObject || m_sceneType == kSceneObjectsGrid)
		{
			EObjectType& objType = m_sceneType == kSceneSingleObject ? m_singleObjScene.objType : m_objsGridScene.objType;
			if (ImGui::Combo("Object type", (int*)&objType, "Sphere\0Cube\0Shader ball\0"))
				m_resetSampling = true;
		}

		if (m_sceneType == kSceneSingleObject)
		{
			const char* materialTypes[] = {"Simple",          "Smooth diffuse", "Rough diffuse", "Smooth conductor",
			                               "Rough conductor", "Rough plastic",  "Texture",       "MERL"};
			if (ImGui::Combo("Material type", (int*)&m_singleObjInstanceData.MaterialType, materialTypes, kMaterialTypesCount))
			{
				if (m_singleObjInstanceData.MaterialType == kMaterialTexture)
					m_material.Load(&m_device, m_singleObjScene.textureMaterial);
				/*if (m_singleObjInstanceData.MaterialType == kMaterialMERL)
				    g_merlMaterial.Load(DXUTGetD3D11Device(), m_singleObjScene.merlMaterial);*/
				m_resetSampling = true;
			}

			if (m_singleObjInstanceData.MaterialType == kMaterialTexture)
			{
				Combo("Material", m_materials, m_singleObjScene.textureMaterial, [this](const char* selected, size_t idx) {
					m_singleObjScene.textureMaterial = selected;
					m_material.Load(&m_device, m_singleObjScene.textureMaterial);
					m_resetSampling = true;
				});
			}
			else if (m_singleObjInstanceData.MaterialType == kMaterialMERL)
			{
				/*Combo("Material", m_merlMaterials.data(), (int)m_merlMaterials.size(), m_singleObjScene.merlMaterial, [this](const char* selected) {
				    m_singleObjScene.merlMaterial = selected;
				    // g_onMaterialChangeCallback();
				});*/
			}
			else
			{
				bool metalnessSlider = false;
				bool roughnessSlider = false;
				bool reflectanceSlider = false;
				bool colorButton = false;
				const char* colorButtonTitle = "Base color";
				bool exportMitsubaButton = false;
				switch (m_singleObjInstanceData.MaterialType)
				{
					case kMaterialSimple:
						metalnessSlider = true;
						roughnessSlider = true;
						reflectanceSlider = true;
						colorButton = true;
						break;
					case kMaterialSmoothDiffuse:
					case kMaterialRoughDiffuse:
						colorButton = true;
						colorButtonTitle = "Albedo";
						if (m_singleObjInstanceData.MaterialType == kMaterialRoughDiffuse)
							roughnessSlider = true;
						exportMitsubaButton = true;
						break;
					case kMaterialSmoothConductor:
					case kMaterialRoughConductor:
						colorButton = true;
						colorButtonTitle = "F0";
						if (m_singleObjInstanceData.MaterialType == kMaterialRoughConductor)
							roughnessSlider = true;
						exportMitsubaButton = true;
						break;
				}

				if (metalnessSlider && ImGui::SliderFloat("Metalness", &m_singleObjInstanceData.Metalness, 0.0f, 1.0f))
					m_resetSampling = true;
				if (roughnessSlider && ImGui::SliderFloat("Roughness", &m_singleObjInstanceData.Roughness, 0.0f, 1.0f))
					m_resetSampling = true;
				if (reflectanceSlider && ImGui::SliderFloat("Reflectance", &m_singleObjInstanceData.Reflectance, 0.0f, 1.0f))
					m_resetSampling = true;
				if (colorButton && ImGui::Button(colorButtonTitle))
				{
					if (ColorDialog(m_singleObjInstanceData.BaseColor))
						m_resetSampling = true;
				}
				if (exportMitsubaButton && ImGui::Button("Export to Mitsuba"))
					ExportToMitsuba();
			}
		}

		if (m_sceneType == kSceneBrdfLobe)
		{
			ImGui::SliderFloat("Metalness", &m_brdfRenderer.metalness, 0.0f, 1.0f);
			ImGui::SliderFloat("Roughness", &m_brdfRenderer.roughness, 0.0f, 1.0f);
			ImGui::SliderFloat("Reflectance", &m_brdfRenderer.reflectance, 0.0f, 1.0f);
			if (ImGui::Button("Color"))
				ColorDialog(m_brdfRenderer.color);
		}

		if (m_sceneType == kSceneObjectsGrid)
		{
			if (ImGui::Button("Base color"))
			{
				if (ColorDialog(m_objsGridInstancesData.front().BaseColor))
				{
					for (size_t i = 1; i < m_objsGridInstancesData.size(); i++)
						m_objsGridInstancesData[i].BaseColor = m_objsGridInstancesData.front().BaseColor;
					m_resetSampling = true;
				}
			}
		}
	}
	ImGui::End();
}
