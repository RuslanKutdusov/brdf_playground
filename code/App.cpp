#include "Precompiled.h"
#include "App.h"
#include "Time.h"

using namespace DirectX;


static const char* kModelsPath[kObjectTypesCount] = {"models\\sphere.obj", "models\\cube.obj", "models\\shader_ball.obj"};


static void AppendXmlLine(std::string& xml, const char* format, ...)
{
	char buf[2048];
	va_list args;
	va_start(args, format);
	int len = vsprintf_s(buf, format, args);
	va_end(args);
	buf[len++] = '\r';
	buf[len++] = '\n';
	xml.append(buf, len);
}


App::App()
{
}


App::~App()
{
	m_device.GetCommandQueue().WaitForIdle();
	for (uint32_t objType = 0; objType < kObjectTypesCount; objType++)
		m_models[objType].Release(&m_device);
	m_material.Release(&m_device);
	m_postProcess.Shutdown();
	m_brdfRenderer.Shutdown();
	m_envEmitter.Shutdown();
	m_envMapFilter.Shutdown();
	m_objRenderer.Shutdown();
	m_imguiWrap.Shutdown();
	m_directLightRT.Release(&m_device);
	m_indirectLightRT.Release(&m_device);
	m_depthTarget.Release(&m_device);
	m_shadowMap.Release(&m_device);
	m_window.Shutdown();
	m_device.Shutdown();
}


bool App::Init()
{
	PIXScopedEvent(0, "App::Init");
	if (!m_device.Init(0))
		return false;

	if (!m_window.Init(&m_device, 1920, 1080, L"BRDF Playground"))
		return false;

	m_window.SetOnWndProcCallback([this](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) { return m_imguiWrap.WndProc(hWnd, message, wParam, lParam); });
	m_window.SetOnResizeCallback([this](uint32_t newWidth, uint32_t newHeight) {
		m_directLightRT.Init(&m_device, DXGI_FORMAT_R16G16B16A16_FLOAT, newWidth, newHeight, L"DirectLightRT", kRenderTargetAllowRTV_DSV);
		m_indirectLightRT.Init(&m_device, DXGI_FORMAT_R32G32B32A32_FLOAT, newWidth, newHeight, L"IndirectLightRT", kRenderTargetAllowRTV_DSV);
		m_depthTarget.Init(&m_device, DXGI_FORMAT_D32_FLOAT, newWidth, newHeight, L"Depth", kRenderTargetAllowRTV_DSV);
		m_resetSampling = true;
	});

	if (!m_imguiWrap.Init(m_window, m_device))
		return false;

	Time::Init();

	memset(&m_globalConstBuffer, 0, sizeof(GlobalConstBuffer));

	RECT windowRect = m_window.GetClientRect();
	m_directLightRT.Init(&m_device, DXGI_FORMAT_R16G16B16A16_FLOAT, windowRect.right, windowRect.bottom, L"DirectLightRT", kRenderTargetAllowRTV_DSV);
	m_indirectLightRT.Init(&m_device, DXGI_FORMAT_R32G32B32A32_FLOAT, windowRect.right, windowRect.bottom, L"IndirectLightRT", kRenderTargetAllowRTV_DSV);
	m_depthTarget.Init(&m_device, DXGI_FORMAT_D32_FLOAT, windowRect.right, windowRect.bottom, L"Depth", kRenderTargetAllowRTV_DSV);
	m_shadowMap.Init(&m_device, DXGI_FORMAT_D16_UNORM, 2048, 2048, L"Shadow map", kRenderTargetAllowRTV_DSV);

	XMVECTOR cameraPos = {-2.f, 3.f, 4.f, 0.f};
	XMVECTOR cameraDir = {0.f, 0.f, 1.f, 0.f};
	XMVECTOR cameraUp = {0.f, 1.f, 0.f, 0.f};
	float aspectRatio = windowRect.right / (float)windowRect.bottom;
	m_orbitCamera.SetOrientation(cameraPos, XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), cameraUp);
	m_orbitCamera.SetNearFarPlanes(0.1f, 3000.0f);
	m_orbitCamera.SetFovY(ToRad(53.4f));
	m_orbitCamera.SetAspectRatio(aspectRatio);
	cameraPos = {0.f, 14.0f, -25.0f, 0.0f};
	cameraDir = XMVector3Normalize(XMVectorScale(cameraPos, -1.0f));
	m_firstPersonCamera.SetOrientation(cameraPos, cameraDir, cameraUp);
	m_firstPersonCamera.SetNearFarPlanes(0.1f, 3000.0f);
	m_firstPersonCamera.SetFovY(ToRad(53.4f));
	m_firstPersonCamera.SetAspectRatio(aspectRatio);

	if (!m_envEmitter.Init(&m_device))
		return false;

	if (!m_envMapFilter.Init(&m_device))
		return false;

	if (!m_objRenderer.Init(&m_device))
		return false;

	if (!m_brdfRenderer.Init(&m_device))
		return false;

	if (!m_postProcess.Init(&m_device))
		return false;

	InitUI();

	for (uint32_t objType = 0; objType < kObjectTypesCount; objType++)
	{
		if (!m_models[objType].Load(&m_device, kModelsPath[objType], false, TextureMapping()))
			return false;
	}

	m_singleObjInstanceData.WorldMatrix = XMMatrixIdentity();
	m_singleObjInstanceData.Metalness = 1.0f;
	m_singleObjInstanceData.Roughness = 0.5f;
	m_singleObjInstanceData.Reflectance = 1.0f;
	m_singleObjInstanceData.BaseColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
	m_singleObjInstanceData.MaterialType = kMaterialSimple;

	for (int x = -5; x <= 5; x++)
	{
		for (int z = -5; z <= 5; z++)
		{
			ObjRenderer::InstanceData data;
			data.WorldMatrix = XMMatrixTranslation(x * 2.5f, 0.0f, z * 2.5f);
			data.Metalness = (x + 5) / 10.0f;
			data.Roughness = (z + 5) / 10.0f;
			data.Reflectance = 1.0f;
			data.BaseColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
			data.MaterialType = kMaterialSimple;
			m_objsGridInstancesData.push_back(data);
		}
	}

	return true;
}


void App::Run()
{
	while (m_window.PeekAndDispatchMessages())
	{
		if (!m_window.IsMinimized())
			OnNewFrame();
	}
}


void App::OnNewFrame()
{
	PIXScopedEvent(0xffffff00, "OnNewFrame");
	Time::NewFrame();
	m_imguiWrap.OnNewFrame();

	PerspectiveCamera* currentCamera = GetCurrentCamera();
	if (!m_imguiWrap.WantMouseAndKeyboard())
	{
		InputEvent inputEvent;
		while (m_window.FetchInputEvent(&inputEvent))
			currentCamera->HandleInputEvent(inputEvent);
		currentCamera->Update(m_window.GetInputState(), Time::GetFrameDeltaTime());
		currentCamera->SetAspectRatio(m_window.GetAspectRatio());
	}
	else
	{
		m_window.ClearInputEventsQueue();
	}

	UpdateUI();

	m_globalConstBuffer.FrameIdx++;

	XMMATRIX viewProj = XMMatrixMultiply(currentCamera->GetWorldToViewMatrix(), currentCamera->GetProjectionMatrix());

	if (m_samplingType == kSamplingTypeIS || m_samplingType == kSamplingTypeFIS)
	{
		if (memcmp(&viewProj, &m_prevFrameViewProj, sizeof(XMMATRIX)) != 0)
			m_resetSampling = true;
	}

	if (m_globalConstBuffer.SamplesProcessed == 0 || m_resetSampling)
	{
		m_prevFrameViewProj = viewProj;
		m_resetSampling = false;
		m_globalConstBuffer.SamplesProcessed = 0;
	}

	UpdateGlobalConstBuffer(viewProj);

	m_device.BeginFrame();
	OnRender();
	m_device.EndFrame();
	m_window.Present(m_vsync ? 1 : 0);

	if (m_samplingType == kSamplingTypeIS || m_samplingType == kSamplingTypeFIS)
	{
		if (m_globalConstBuffer.SamplesProcessed < m_samplesCount)
			m_globalConstBuffer.SamplesProcessed += m_samplesPerFrame;
	}
	else
	{
		m_globalConstBuffer.SamplesProcessed = m_samplesCount;
	}
}


void App::OnRender()
{
	ID3D12GraphicsCommandList* cmdList = m_device.GetCommandList();

	D3D12_GPU_VIRTUAL_ADDRESS globalCb = m_device.UpdateConstantBuffer(&m_globalConstBuffer, sizeof(m_globalConstBuffer));
	cmdList->SetGraphicsRootConstantBufferView(0, globalCb);
	cmdList->SetComputeRootConstantBufferView(0, globalCb);

	ID3D12Resource* swapChainBuffer = m_window.GetCurrentBuffer();
	RTVHandle swapChainRTV = m_window.GetCurrentRTV();

	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	barriers.push_back(TransitionBarrier(swapChainBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	m_depthTarget.TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE, barriers);
	cmdList->ResourceBarrier(barriers.size(), barriers.data());
	barriers.clear();

	float clearColor[4] = {0.3f, 0.2f, 0.4f, 0.0f};
	cmdList->ClearRenderTargetView(swapChainRTV, clearColor, 0, nullptr);
	cmdList->ClearDepthStencilView(m_depthTarget.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	if (m_sceneType == kSceneBrdfLobe)
	{
		cmdList->OMSetRenderTargets(1, &swapChainRTV, false, &m_depthTarget.dsv);
		RECT windowRect = m_window.GetClientRect();
		SetViewportAndScissorRect(cmdList, windowRect.right, windowRect.bottom);
		m_brdfRenderer.Render(cmdList, m_globalConstBuffer.LightDir);
	}
	else
	{
		m_directLightRT.TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET, barriers);
		m_indirectLightRT.TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET, barriers);
		cmdList->ResourceBarrier(barriers.size(), barriers.data());
		barriers.clear();

		m_envEmitter.BakeCubemap(cmdList);
		if (m_samplingType == kSamplingTypeBakedSplitSumNV && m_globalConstBuffer.SamplesProcessed == 0)
			m_envMapFilter.FilterEnvMap(cmdList, m_envEmitter.GetCubeMapSRV());
		RenderScene(cmdList);

		m_directLightRT.TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, barriers);
		m_indirectLightRT.TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, barriers);
		cmdList->ResourceBarrier(barriers.size(), barriers.data());
		barriers.clear();

		cmdList->OMSetRenderTargets(1, &swapChainRTV, false, nullptr);
		RECT windowRect = m_window.GetClientRect();
		SetViewportAndScissorRect(cmdList, windowRect.right, windowRect.bottom);
		m_postProcess.Render(cmdList, m_directLightRT.srv, m_indirectLightRT.srv);
	}

	m_imguiWrap.RenderDrawData();

	D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(swapChainBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	cmdList->ResourceBarrier(1, &barrier);
}


void App::UpdateGlobalConstBuffer(const XMMATRIX& viewProj)
{
	m_globalConstBuffer.PrefilteredDiffuseEnvMap = m_envMapFilter.GetPrefilteredDiffEnvMap().idx;
	m_globalConstBuffer.BRDFLut = m_envMapFilter.GetBRDFLut().idx;
	m_globalConstBuffer.PrefilteredSpecularEnvMap = m_envMapFilter.GetPrefilteredSpecEnvMap().idx;
	m_globalConstBuffer.ShadowMap = m_shadowMap.srv.idx;
	m_globalConstBuffer.EnvironmentMap = m_envEmitter.GetCubeMapSRV().idx;
	m_globalConstBuffer.ViewProjMatrix = viewProj;
	m_globalConstBuffer.ViewPos = GetCurrentCamera()->GetPosition();
	float lightDirVert = ToRad(m_lightDirVert);
	float lightDirHor = ToRad(m_lightDirHor);
	m_globalConstBuffer.LightDir = XMVectorSet(sin(lightDirVert) * sin(lightDirHor), cos(lightDirVert), sin(lightDirVert) * cos(lightDirHor), 0.0f);
	m_globalConstBuffer.LightIlluminance = XMVectorScale(m_lightColor, m_lightIlluminance);
	m_globalConstBuffer.SamplingType = m_samplingType;
	m_globalConstBuffer.TotalSamples = m_samplesCount;
	m_globalConstBuffer.SamplesPerFrame = m_samplesPerFrame;
	m_globalConstBuffer.EnableDirectLight = m_enableDirectLight;
	m_globalConstBuffer.EnableEnvEmitter = m_enableEnvEmitter;
	m_globalConstBuffer.EnableShadow = m_enableShadow;
	m_globalConstBuffer.EnableDiffuseBRDF = m_enableDiffuseBRDF;
	m_globalConstBuffer.EnableSpecularBRDF = m_enableSpecularBRDF;
	RECT windowRect = m_window.GetClientRect();
	m_globalConstBuffer.ScreenWidth = windowRect.right;
	m_globalConstBuffer.ScreenHeight = windowRect.bottom;

	// update shadow matrix
	const float width = 40.0f;
	const float height = 40.0f;
	const float depthRange = 100.0f;
	XMVECTOR shadowCameraPos = XMVectorScale(m_globalConstBuffer.LightDir, 0.5f * depthRange);

	XMMATRIX viewMatrix, projMatrix;
	viewMatrix = XMMatrixLookAtLH(shadowCameraPos, XMVectorZero(), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	projMatrix = XMMatrixOrthographicLH(width, height, 1.0f, depthRange);

	XMMATRIX lsOffset;
	lsOffset.r[0] = XMVectorSet(0.5f, 0.0f, 0.0f, 0.0f);
	lsOffset.r[1] = XMVectorSet(0.0f, -0.5f, 0.0f, 0.0f);
	lsOffset.r[2] = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	lsOffset.r[3] = XMVectorSet(0.5f, 0.5f, 0.0f, 1.0f);

	m_globalConstBuffer.ShadowViewProjMatrix = XMMatrixMultiply(viewMatrix, projMatrix);
	m_globalConstBuffer.ShadowMatrix = XMMatrixMultiply(m_globalConstBuffer.ShadowViewProjMatrix, lsOffset);
}


PerspectiveCamera* App::GetCurrentCamera()
{
	if (m_sceneType == kSceneSingleObject || m_sceneType == kSceneBrdfLobe)
		return &m_orbitCamera;
	return &m_firstPersonCamera;
}


void App::RenderScene(ID3D12GraphicsCommandList* cmdList)
{
	// shadow pass
	PIXBeginEvent(cmdList, 0, "Shadow pass");
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	m_shadowMap.TransitionTo(D3D12_RESOURCE_STATE_DEPTH_WRITE, barriers);
	cmdList->ResourceBarrier(barriers.size(), barriers.data());
	barriers.clear();
	cmdList->OMSetRenderTargets(0, nullptr, false, &m_shadowMap.dsv);
	cmdList->ClearDepthStencilView(m_shadowMap.dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	SetViewportAndScissorRect(cmdList, m_shadowMap.m_width, m_shadowMap.m_height);

	const Model* model = nullptr;
	const ObjRenderer::InstanceData* instancesData = nullptr;
	uint32_t instancesNum = 0;
	if (m_sceneType == kSceneSingleObject)
	{
		model = &m_models[m_singleObjScene.objType];
		instancesData = &m_singleObjInstanceData;
		instancesNum = 1;
	}
	else if (m_sceneType == kSceneObjectsGrid)
	{
		model = &m_models[m_objsGridScene.objType];
		instancesData = m_objsGridInstancesData.data();
		instancesNum = (uint32_t)m_objsGridInstancesData.size();
	}
	m_objRenderer.RenderShadowPass(cmdList, model, instancesData, instancesNum);

	m_shadowMap.TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, barriers);
	cmdList->ResourceBarrier(barriers.size(), barriers.data());
	barriers.clear();
	PIXEndEvent(cmdList);

	// depth + light passes
	RTVHandle rtvs[] = {m_directLightRT.rtv, m_indirectLightRT.rtv};
	float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	cmdList->OMSetRenderTargets(2, rtvs, false, &m_depthTarget.dsv);
	cmdList->ClearRenderTargetView(m_directLightRT.rtv, clearColor, 0, nullptr);
	if (m_globalConstBuffer.SamplesProcessed == 0)
		cmdList->ClearRenderTargetView(m_indirectLightRT.rtv, clearColor, 0, nullptr);

	RECT windowRect = m_window.GetClientRect();
	SetViewportAndScissorRect(cmdList, windowRect.right, windowRect.bottom);

	// depth pass
	PIXBeginEvent(cmdList, 0, "Depth pass");
	m_envEmitter.RenderDepthPass(cmdList);
	m_objRenderer.RenderDepthPass(cmdList, model, instancesData, instancesNum);
	PIXEndEvent(cmdList);

	// light pass
	PIXBeginEvent(cmdList, 0, "Light pass");
	m_envEmitter.RenderLightPass(cmdList);
	m_objRenderer.RenderLightPass(cmdList, model, instancesData, instancesNum, &m_material);
	PIXEndEvent(cmdList);
}


void App::ExportToMitsuba()
{
	const Model* model = nullptr;
	const ObjRenderer::InstanceData* instancesData = nullptr;
	uint32_t instancesNum = 0;
	if (m_sceneType == kSceneSingleObject)
	{
		model = &m_models[m_singleObjScene.objType];
		instancesData = &m_singleObjInstanceData;
		instancesNum = 1;
	}
	else if (m_sceneType == kSceneObjectsGrid)
	{
		model = &m_models[m_objsGridScene.objType];
		instancesData = m_objsGridInstancesData.data();
		instancesNum = (uint32_t)m_objsGridInstancesData.size();
	}

	std::string xml;

	const char* epilog =
	    "<?xml version='1.0' encoding='utf-8'?>\r\n"
	    "<scene version=\"0.5.0\">\r\n"
	    "	<integrator type=\"path\"/>\r\n";
	xml.append(epilog);

	for (uint32_t i = 0; i < instancesNum; i++)
	{
		const ObjRenderer::InstanceData& instance = instancesData[i];
		char buf[256];

		AppendXmlLine(xml, "	<shape type=\"obj\" id=\"Obj%u\">", i);
		AppendXmlLine(xml, "		<string name=\"filename\" value = \"data\\%s\"/>", model->name.c_str());

		AppendXmlLine(xml, "		<transform name=\"toWorld\">");
		xml.append("			<matrix value=\"");
		float matrix[16];
		memcpy(matrix, &instance.WorldMatrix, sizeof(matrix));
		for (uint32_t j = 0; j < 16; j++)
		{
			sprintf(buf, "%f ", matrix[j]);
			xml.append(buf);
		}
		xml.append("\"/>\r\n");
		AppendXmlLine(xml, "		</transform>");

		if (instance.MaterialType == kMaterialSmoothDiffuse || instance.MaterialType == kMaterialRoughDiffuse)
		{
			float diffuseReflectance[3];
			memcpy(diffuseReflectance, &instance.BaseColor, sizeof(diffuseReflectance));

			if (instance.MaterialType == kMaterialSmoothDiffuse)
			{
				AppendXmlLine(xml, "		<bsdf type=\"diffuse\">");
			}
			else if (instance.MaterialType == kMaterialRoughDiffuse)
			{
				AppendXmlLine(xml, "		<bsdf type=\"roughdiffuse\">");
				AppendXmlLine(xml, "			<float name=\"alpha\" value=\"%f\"/>", instance.Roughness * instance.Roughness);
			}
			AppendXmlLine(xml, "			<rgb name=\"reflectance\" value=\"%f, %f, %f\"/>", diffuseReflectance[0], diffuseReflectance[1],
			              diffuseReflectance[2]);
			AppendXmlLine(xml, "		</bsdf>");
		}
		else if (instance.MaterialType == kMaterialSmoothConductor || instance.MaterialType == kMaterialRoughConductor)
		{
		}
		else if (instance.MaterialType == kMaterialRoughPlastic)
		{
			const float DIELECTRIC_SPEC = 0.04f;
			float dielectricSpec = DIELECTRIC_SPEC * instance.Reflectance;
			float oneMinusReflectivity = (1.0f - dielectricSpec);  // *(1.0f - instance.Metalness);
			float F0 = dielectricSpec;                             // lerp(dielectricSpec, baseColor, metalness);
			float IOR = (1.0f + sqrt(F0)) / (1.0f - sqrt(F0));
			DirectX::XMVECTOR diffuseReflectanceVec = XMVectorScale(instance.BaseColor, oneMinusReflectivity);
			float diffuseReflectance[3];
			memcpy(diffuseReflectance, &diffuseReflectanceVec, sizeof(diffuseReflectance));

			AppendXmlLine(xml, "		<bsdf type=\"roughplastic\">");
			AppendXmlLine(xml, "			<string name=\"distribution\" value=\"ggx\"/>");
			AppendXmlLine(xml, "			<float name=\"alpha\" value=\"%f\"/>", instance.Roughness * instance.Roughness);
			AppendXmlLine(xml, "			<rgb name=\"diffuseReflectance\" value=\"%f, %f, %f\"/>", diffuseReflectance[0], diffuseReflectance[1],
			              diffuseReflectance[2]);
			AppendXmlLine(xml, "			<float name=\"intIOR\" value=\"%f\"/>", IOR);
			AppendXmlLine(xml, "		</bsdf>");
		}

		AppendXmlLine(xml, "	</shape>");
	}

	if (m_envEmitter.GetType() == EnvEmitter::kTypeConstLuminance)
	{
		float radiance[3];
		memcpy(radiance, &m_envEmitter.GetConstLuminanceColor(), sizeof(radiance));
		radiance[0] *= m_envEmitter.GetConstLuminance();
		radiance[1] *= m_envEmitter.GetConstLuminance();
		radiance[2] *= m_envEmitter.GetConstLuminance();

		AppendXmlLine(xml, "	<emitter type=\"constant\">");
		AppendXmlLine(xml, "		<rgb name=\"radiance\" value=\"%f, %f, %f\"/>", radiance[0], radiance[1], radiance[2]);
		AppendXmlLine(xml, "	</emitter>");
	}
	else
	{
		AppendXmlLine(xml, "	<emitter type=\"envmap\">");
		AppendXmlLine(xml, "		<string name=\"filename\" value=\"data\\HDRs\\%s\"/>", m_envEmitter.GetTextureFileName());
		AppendXmlLine(xml, "		<float name=\"scale\" value=\"%f\"/>", m_envEmitter.GetScale());
		AppendXmlLine(xml, "		<boolean name=\"cache\" value=\"false\"/>");
		AppendXmlLine(xml, "	</emitter>");
	}

	PerspectiveCamera* camera = GetCurrentCamera();
	Float4 lookAt = ToFloat4(camera->GetPosition() + camera->GetDirection());
	Float4 pos = ToFloat4(camera->GetPosition());
	Float4 up = ToFloat4(camera->GetUp());
	RECT rect = m_window.GetClientRect();

	AppendXmlLine(xml, "	<sensor type=\"perspective\">");
	AppendXmlLine(xml, "		<float name=\"farClip\" value=\"%f\"/>", camera->GetFarZ());
	AppendXmlLine(xml, "		<float name=\"nearClip\" value=\"%f\"/>", camera->GetNearZ());
	AppendXmlLine(xml, "		<float name=\"fov\" value=\"%f\"/>", ToDeg(camera->GetFovY()));
	AppendXmlLine(xml, "		<string name=\"fovAxis\" value=\"y\"/>");
	AppendXmlLine(xml, "		<transform name=\"toWorld\">");
	AppendXmlLine(xml, "			<lookat target = \"%f, %f, %f\" origin = \"%f, %f, %f\" up = \"%f, %f, %f\" />", lookAt.x, lookAt.y, -lookAt.z, pos.x,
	              pos.y, -pos.z, up.x, up.y, -up.z);
	AppendXmlLine(xml, "		</transform>");
	AppendXmlLine(xml, "		<sampler type = \"ldsampler\">");
	AppendXmlLine(xml, "			<integer name = \"sampleCount\" value = \"256\" />");
	AppendXmlLine(xml, "		</sampler>");
	AppendXmlLine(xml, "		<film type = \"ldrfilm\">");
	AppendXmlLine(xml, "			<integer name = \"width\" value = \"%u\" />", rect.right);
	AppendXmlLine(xml, "			<integer name = \"height\" value = \"%u\" />", rect.bottom);
	float exposure = -(m_postProcess.ev100 - log2f(1.2f));
	AppendXmlLine(xml, "			<float name = \"exposure\" value = \"%f\" />", exposure);
	AppendXmlLine(xml, "			<rfilter type = \"gaussian\" />");
	AppendXmlLine(xml, "		</film>");
	AppendXmlLine(xml, "	</sensor>");
	AppendXmlLine(xml, "</scene>");

	FILE* f = fopen("mitsuba.xml", "wb");
	fwrite(xml.data(), 1, xml.length(), f);
	fclose(f);
}


int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	char szFileName[MAX_PATH];
	GetModuleFileNameA(NULL, szFileName, MAX_PATH);
	FilePath workingDir = FilePath(szFileName).GetParentPath();
	SetCurrentDirectoryA(workingDir.c_str());

	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);
	if (argc > 1 && wcscmp(argv[0], L"texconv") == 0)
	{
		if (argc < 2)
			return -1;

		Material::ETextures type = Material::kTexturesCount;
		if (wcscmp(argv[1], L"basecolor") == 0)
			type = Material::kBaseColor;
		else if (wcscmp(argv[1], L"normal") == 0)
			type = Material::kNormal;
		else if (wcscmp(argv[1], L"roughness") == 0)
			type = Material::kRoughness;
		else if (wcscmp(argv[1], L"metalness") == 0)
			type = Material::kMetalness;
		else if (wcscmp(argv[1], L"ao") == 0)
			type = Material::kAO;

		if (type == Material::kTexturesCount)
		{
			LogStdErr("Unknown texture type '%S'\n", argv[1]);
			return -1;
		}

		DXGI_FORMAT format;
		DXGI_FORMAT compressedFormat;
		bool srgbIn = false;
		switch (type)
		{
			case Material::kBaseColor:
				format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				compressedFormat = DXGI_FORMAT_BC3_UNORM_SRGB;
				srgbIn = true;
				break;
			case Material::kAO:
				format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				compressedFormat = DXGI_FORMAT_BC4_UNORM;
				break;
			case Material::kNormal:
				format = DXGI_FORMAT_R8G8B8A8_UNORM;
				compressedFormat = DXGI_FORMAT_BC5_UNORM;
				break;
			case Material::kRoughness:
			case Material::kMetalness:
				format = DXGI_FORMAT_R8_UNORM;
				compressedFormat = DXGI_FORMAT_BC4_UNORM;
				break;
			case Material::kTexturesCount:
				break;
			default:
				break;
		}

		FilePathW input = argv[2];

		TexMetadata data;
		ScratchImage image;
		if (!LoadTexture(input, &data, image))
		{
			LogStdErr("Failed to load texture\n");
			return -1;
		}

		ScratchImage convertedImage;
		if (format != data.format)
		{
			TEX_FILTER_FLAGS flags = srgbIn ? TEX_FILTER_SRGB_IN : TEX_FILTER_DEFAULT;
			if (FAILED(Convert(*image.GetImage(0, 0, 0), format, flags, 0.0f, convertedImage)))
			{
				LogStdErr("Failed to convert texture\n");
				return -1;
			}
		}
		else
		{
			convertedImage = std::move(image);
		}

		ScratchImage mipChain;
		if (FAILED(GenerateMipMaps(*convertedImage.GetImage(0, 0, 0), TEX_FILTER_BOX, 0, mipChain)))
		{
			LogStdErr("Failed to generate mips\n");
			return -1;
		}

		ScratchImage compressedMipChain;
		Compress(mipChain.GetImages(), mipChain.GetImageCount(), mipChain.GetMetadata(), compressedFormat, TEX_COMPRESS_PARALLEL, 0.0f, compressedMipChain);

		FilePathW output = input.GetParentPath();
		output /= argv[1];
		output.SetExtension(L".dds");
		if (FAILED(SaveToDDSFile(compressedMipChain.GetImages(), compressedMipChain.GetImageCount(), compressedMipChain.GetMetadata(), DDS_FLAGS_FORCE_DX10_EXT,
		                         output.c_str())))
		{
			LogStdErr("Failed to save output file '%S'\n", output.c_str());
			return -1;
		}

		return 0;
	}

	App app;
	if (!app.Init())
		return -1;
	app.Run();
	return 0;
}