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


static bool FileNameEndingMatch(const FilePath& name, const char* ending)
{
	size_t len = strlen(ending);
	if (name.length() <= len)
		return false;
	size_t offset = name.length() - len;
	return strcmp(name.data() + offset, ending) == 0;
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

	std::vector<FilePath> spdFiles;
	EnumerateFiles("data\\SPDs\\*", spdFiles);
	for (size_t i = 0; i < spdFiles.size();)
	{
		const char* eta = ".eta.spd";
		const char* k = ".k.spd";
		if (i + 1 < spdFiles.size() && FileNameEndingMatch(spdFiles[i], eta) && FileNameEndingMatch(spdFiles[i + 1], k))
		{
			m_spdFiles.push_back({spdFiles[i].data(), (uint32_t)(spdFiles[i].length() - strlen(eta))});
			i += 2;
		}
		else
			i++;
	}
	if (!m_spdFiles.empty())
		m_singleObjScene.ior = m_spdFiles[0].c_str();

	EnumerateFiles("data\\MERL\\*", m_merlMaterials);
	if (!m_merlMaterials.empty())
		m_singleObjScene.merlMaterial = m_merlMaterials[0].c_str();

	m_fresnelWindow.Init(m_spdFiles);
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
		ImGui::SameLine();
		if (ImGui::Button("Fresnel viewer"))
			m_fresnelWindow.Show();
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
		if (ImGui::Combo("Scene", (int*)&m_sceneType, "Single object\0Objects grid\0BRDF\0\0"))
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
				if (m_singleObjInstanceData.MaterialType == kMaterialSmoothConductor || m_singleObjInstanceData.MaterialType == kMaterialRoughConductor)
					m_singleObjInstanceData.BaseColor = ComputeF0(m_singleObjScene.ior);
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
				bool iorCombo = false;
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
						iorCombo = true;
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
				if (iorCombo)
				{
					Combo("IOR", m_spdFiles, m_singleObjScene.ior, [this](const char* selected, size_t idx) {
						m_singleObjScene.ior = selected;
						m_singleObjInstanceData.BaseColor = ComputeF0(selected);
						m_resetSampling = true;
					});
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

	m_fresnelWindow.Update();
}


void FresnelWindow::Init(const std::vector<FilePath>& spdFiles)
{
	m_spdFiles = spdFiles;
	if (!m_spdFiles.empty())
		LoadSPDs(m_spdFiles.front().c_str());
}


void FresnelWindow::Show()
{
	m_opened = true;
}


void FresnelWindow::Update()
{
	if (!m_opened)
		return;

	if (ImGui::Begin("Fresnel", &m_opened))
	{
		Combo("IOR", m_spdFiles, selectorIOR, [this](const char* selected, size_t idx) { LoadSPDs(selected); });
		const char* plotTypes[] = {"Fresnel", "IOR"};
		ImGui::Combo("Plot type", (int*)&m_plotType, plotTypes, kPlotTypesCount);

		if (m_plotType == kFresnelPlot)
			DrawFresnelPlot();
		else if (m_plotType == kIORPlot)
			DrawIORPlot();
	}
	ImGui::End();
}


void FresnelWindow::BuildFresnelPlot(float plotCanvasWidth)
{
	const float kPlotStepLength = 5.0f;
	uint32_t plotStepsNum = (uint32_t)((plotCanvasWidth + kPlotStepLength - 1.0f) / kPlotStepLength);

	if (m_needToBuildPlot || m_accuratePlot.size() != plotStepsNum)
	{
		m_accuratePlot.resize(plotStepsNum);
		m_schlickPlot.resize(plotStepsNum);
		float angleStep = XM_PI * 0.5f / plotStepsNum;
		for (size_t i = 0; i < plotStepsNum; i++)
		{
			float angle = angleStep * i;
			float cosTheta = cosf(angle);

			Spectrum spectrum = FresnelConductorExact(cosTheta - 1e-06f, m_etaSpectrum, m_kSpectrum);
			spectrum *= GetD65();
			XMFLOAT3 color;
			spectrum.ToLinearRGB(color.x, color.y, color.z);
			m_accuratePlot[i] = XMLoadFloat3(&color);

			float fc = powf(1.0f - cosTheta, 5.0f);
			m_schlickPlot[i] = XMVectorAdd(XMVectorScale(m_accuratePlot[0], 1.0f - fc), XMVectorSet(fc, fc, fc, fc));
		}
		m_needToBuildPlot = false;
	}
}


void FresnelWindow::DrawFresnelPlot()
{
	ImGui::Checkbox("Accurate", &m_drawAccuratePlot);
	ImGui::Checkbox("Schlick", &m_drawSchlickPlot);
	ImGui::Checkbox("R", &m_plotDrawRGB[0]);
	ImGui::SameLine();
	ImGui::Checkbox("G", &m_plotDrawRGB[1]);
	ImGui::SameLine();
	ImGui::Checkbox("B", &m_plotDrawRGB[2]);

	float reflDigitsWidth = ImGui::CalcTextSize("0.0").x;

	ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
	ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
	if (canvas_sz.x < 50.0f)
		canvas_sz.x = 50.0f;
	if (canvas_sz.y < 50.0f)
		canvas_sz.y = 50.0f;
	ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);
	ImVec2 plot_p0 = {canvas_p0.x + reflDigitsWidth, canvas_p0.y};
	ImVec2 plot_p1 = {canvas_p1.x, canvas_p1.y - ImGui::GetFontSize()};
	ImVec2 plot_sz = {plot_p1.x - plot_p0.x, plot_p1.y - plot_p0.y};

	BuildFresnelPlot(plot_sz.x);

	ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

	// Draw border and background color
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
	draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

	for (float y = 0.0f; y <= 1.0f; y += 0.2f)
	{
		char buf[16];
		sprintf(buf, "%.1f", y);
		float lineY = plot_p0.y + plot_sz.y - y * plot_sz.y;
		float textY = lineY - ImGui::GetFontSize() * 0.5f;
		textY = std::min(textY, plot_p1.y - ImGui::GetFontSize());
		textY = std::max(textY, plot_p0.y);
		draw_list->AddText({canvas_p0.x, textY}, IM_COL32(0xff, 0xff, 0xff, 0xff), buf);
		draw_list->AddLine({plot_p0.x, lineY}, {plot_p1.x, lineY}, IM_COL32(0x7f, 0x7f, 0x7f, 0xff));
	}

	for (float angle = 0.0f; angle <= 90.0f; angle += 15.0f)
	{
		float lineX = plot_p0.x + plot_sz.x * ToRad(angle) / XM_PI * 2.0f;
		draw_list->AddLine({lineX, plot_p0.y}, {lineX, plot_p1.y}, IM_COL32(0x7f, 0x7f, 0x7f, 0xff));

		char buf[16];
		sprintf(buf, "%.0f", angle);
		float textWidth = ImGui::CalcTextSize(buf).x;
		float textX = lineX - textWidth * 0.5f;
		textX = std::min(textX, plot_p1.x - textWidth);
		textX = std::max(textX, plot_p0.x);
		draw_list->AddText({textX, plot_p1.y}, IM_COL32(0xff, 0xff, 0xff, 0xff), buf);
	}

	float xScale = plot_sz.x / (m_accuratePlot.size() - 1);
	float yScale = plot_sz.y;

	auto drawLine = [&plot_p0, &plot_sz, xScale, yScale, draw_list, this](size_t i, const std::vector<XMVECTOR>& plot, uint32_t channel, uint32_t color) {
		if (!m_plotDrawRGB[channel])
			return;

		ImVec2 p0 = plot_p0;
		p0.x += i * xScale;
		p0.y += plot_sz.y - XMVectorGetByIndex(plot[i], channel) * yScale;

		ImVec2 p1 = plot_p0;
		p1.x += (i + 1) * xScale;
		p1.y += plot_sz.y - XMVectorGetByIndex(plot[i + 1], channel) * yScale;

		draw_list->AddLine(p0, p1, color);
	};

	for (size_t i = 0; i < m_accuratePlot.size() - 1; i++)
	{
		if (m_drawAccuratePlot)
		{
			drawLine(i, m_accuratePlot, 0, IM_COL32(0xff, 0, 0, 0xff));
			drawLine(i, m_accuratePlot, 1, IM_COL32(0, 0xff, 0, 0xff));
			drawLine(i, m_accuratePlot, 2, IM_COL32(0, 0, 0xff, 0xff));
		}

		if (m_drawSchlickPlot)
		{
			drawLine(i, m_schlickPlot, 0, IM_COL32(0x7f, 0, 0, 0xff));
			drawLine(i, m_schlickPlot, 1, IM_COL32(0, 0x7f, 0, 0xff));
			drawLine(i, m_schlickPlot, 2, IM_COL32(0, 0, 0x7f, 0xff));
		}
	}
}


void FresnelWindow::DrawIORPlot()
{
	float reflDigitsWidth = ImGui::CalcTextSize("0.0").x;

	ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
	ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
	if (canvas_sz.x < 50.0f)
		canvas_sz.x = 50.0f;
	if (canvas_sz.y < 50.0f)
		canvas_sz.y = 50.0f;
	ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);
	ImVec2 plot_p0 = {canvas_p0.x + reflDigitsWidth, canvas_p0.y};
	ImVec2 plot_p1 = {canvas_p1.x, canvas_p1.y - ImGui::GetFontSize()};
	ImVec2 plot_sz = {plot_p1.x - plot_p0.x, plot_p1.y - plot_p0.y};

	ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

	// Draw border and background color
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
	draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

	float maxY = std::max(m_etaSpectrumMaxVal, m_kSpectrumMaxVal);
	maxY = std::ceilf(maxY);

	const uint32_t kHorizontalLinesNum = 10;
	for (uint32_t i = 0; i < kHorizontalLinesNum; i++)
	{
		float y = (float)i / (kHorizontalLinesNum - 1) * maxY;
		char buf[16];
		sprintf(buf, "%.1f", y);
		float lineY = plot_p0.y + plot_sz.y - (y / maxY) * plot_sz.y;
		float textY = lineY - ImGui::GetFontSize() * 0.5f;
		textY = std::min(textY, plot_p1.y - ImGui::GetFontSize());
		textY = std::max(textY, plot_p0.y);
		draw_list->AddText({canvas_p0.x, textY}, IM_COL32(0xff, 0xff, 0xff, 0xff), buf);
		draw_list->AddLine({plot_p0.x, lineY}, {plot_p1.x, lineY}, IM_COL32(0x7f, 0x7f, 0x7f, 0xff));
	}

	const uint32_t kVertLinesNum = 10;
	for (uint32_t i = 0; i < kVertLinesNum; i++)
	{
		float scale = (float)i / (kVertLinesNum - 1);
		float lineX = plot_p0.x + plot_sz.x * scale;
		draw_list->AddLine({lineX, plot_p0.y}, {lineX, plot_p1.y}, IM_COL32(0x7f, 0x7f, 0x7f, 0xff));

		char buf[16];
		float lambda = kSpectrumMinWavelength + scale * kSpectrumRange;
		sprintf(buf, "%.0f", lambda);
		float textWidth = ImGui::CalcTextSize(buf).x;
		float textX = lineX - textWidth * 0.5f;
		textX = std::min(textX, plot_p1.x - textWidth);
		textX = std::max(textX, plot_p0.x);
		draw_list->AddText({textX, plot_p1.y}, IM_COL32(0xff, 0xff, 0xff, 0xff), buf);
	}

	float xScale = plot_sz.x / (kSpectrumSamples - 1);
	float yScale = plot_sz.y;

	auto drawLine = [&plot_p0, &plot_sz, xScale, yScale, draw_list, maxY](uint32_t i, const Spectrum& spectrum, uint32_t color) {
		ImVec2 p0 = plot_p0;
		p0.x += i * xScale;
		p0.y += plot_sz.y - spectrum[i] / maxY * yScale;

		ImVec2 p1 = plot_p0;
		p1.x += (i + 1) * xScale;
		p1.y += plot_sz.y - spectrum[i + 1] / maxY * yScale;

		draw_list->AddLine(p0, p1, color);
	};

	for (uint32_t i = 0; i < kSpectrumSamples - 1; i++)
	{
		drawLine(i, m_etaSpectrum, IM_COL32(0xff, 0xff, 0, 0xff));
		drawLine(i, m_kSpectrum, IM_COL32(0, 0xff, 0xff, 0xff));
	}
}


void FresnelWindow::LoadSPDs(const char* path)
{
	selectorIOR = path;

	FilePath filename = path;
	filename.SetExtension(".eta.spd");
	SpectralPowerDistribution etaSPD;
	etaSPD.InitFromFile(filename.c_str());

	m_etaSpectrum = Spectrum(etaSPD);
	m_etaSpectrumMaxVal = -FLT_MAX;
	for (uint32_t i = 0; i < m_etaSpectrum.Size(); i++)
		m_etaSpectrumMaxVal = std::max(m_etaSpectrumMaxVal, m_etaSpectrum[i]);

	filename = path;
	filename.SetExtension(".k.spd");
	SpectralPowerDistribution kSPD;
	kSPD.InitFromFile(filename.c_str());

	m_kSpectrum = Spectrum(kSPD);
	m_kSpectrumMaxVal = -FLT_MAX;
	for (uint32_t i = 0; i < m_kSpectrum.Size(); i++)
		m_kSpectrumMaxVal = std::max(m_kSpectrumMaxVal, m_kSpectrum[i]);

	m_needToBuildPlot = true;
}