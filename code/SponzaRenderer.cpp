#include "Precompiled.h"

#if 0
struct InstanceParams
{
	XMVECTOR PointLightPos;
	XMVECTOR PointLightFlux;
};


SponzaRenderer::SponzaRenderer()
{
}


HRESULT SponzaRenderer::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;
	TextureMapping textureMapping;
	textureMapping[kTextureDiffuse] = aiTextureType_DIFFUSE;
	textureMapping[kTextureNormal] = aiTextureType_HEIGHT;
	textureMapping[kTextureRoughness] = aiTextureType_SHININESS;
	textureMapping[kTextureMetalness] = aiTextureType_AMBIENT;
	V_RETURN(m_model.Load(pd3dDevice, L"sponza", L"sponza.obj", true, textureMapping));
	V_RETURN(ReloadShaders(pd3dDevice));

	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;
	Desc.ByteWidth = sizeof(InstanceParams);
	V_RETURN(pd3dDevice->CreateBuffer(&Desc, nullptr, &m_instanceBuf));
	DXUT_SetDebugName(m_instanceBuf, "InstanceParams");

	return S_OK;
}


void SponzaRenderer::Render(ID3D11DeviceContext* pd3dImmediateContext, bool depthPass)
{
	pd3dImmediateContext->IASetInputLayout(m_inputLayout);
	pd3dImmediateContext->VSSetShader(m_vs, nullptr, 0);
	pd3dImmediateContext->PSSetShader(depthPass ? nullptr : m_ps, nullptr, 0);

	for (uint32_t i = 0; i < m_model.meshesNum; i++)
	{
		Mesh& mesh = m_model.meshes[i];
		mesh.Apply(pd3dImmediateContext);

		if (!depthPass)
		{
			Material& m = m_model.materials[mesh.materialIdx];
			pd3dImmediateContext->PSSetShaderResources(0, 1, &m.baseColor);
			pd3dImmediateContext->PSSetShaderResources(1, 1, &m.normal);
			pd3dImmediateContext->PSSetShaderResources(2, 1, &m.roughness);
			pd3dImmediateContext->PSSetShaderResources(3, 1, &m.metalness);
		}

		pd3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pd3dImmediateContext->DrawIndexed(mesh.indexCount, 0, 0);
	}
}


void SponzaRenderer::RenderDepthPass(ID3D11DeviceContext* pd3dImmediateContext)
{
	Render(pd3dImmediateContext, true);
}


void SponzaRenderer::RenderLightPass(ID3D11DeviceContext* pd3dImmediateContext, const XMVECTOR& pointLightFlux, float fTime)
{
	D3D11_MAPPED_SUBRESOURCE mappedSubres;
	pd3dImmediateContext->Map(m_instanceBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
	InstanceParams* instanceParams = (InstanceParams*)mappedSubres.pData;

	XMVECTOR initialPos = XMVectorSet(0.0f, 2.0f, 0.0f, 0.0f);
	XMVECTOR moveDir = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	instanceParams->PointLightPos = XMVectorAdd(initialPos, XMVectorScale(moveDir, cos(fTime * 1.0f) * 15.0f));
	instanceParams->PointLightFlux = pointLightFlux;

	memcpy(mappedSubres.pData, instanceParams, sizeof(InstanceParams));
	pd3dImmediateContext->Unmap(m_instanceBuf, 0);

	// apply
	pd3dImmediateContext->VSSetConstantBuffers(1, 1, &m_instanceBuf);
	pd3dImmediateContext->PSSetConstantBuffers(1, 1, &m_instanceBuf);

	Render(pd3dImmediateContext, false);
}


void SponzaRenderer::OnD3D11DestroyDevice()
{
	m_model.Release();
	SAFE_RELEASE(m_inputLayout);
	SAFE_RELEASE(m_vs);
	SAFE_RELEASE(m_ps);
	SAFE_RELEASE(m_instanceBuf);
}


HRESULT SponzaRenderer::ReloadShaders(ID3D11Device* pd3dDevice)
{
	HRESULT hr;
	ID3D11VertexShader* newVs = nullptr;
	ID3D11PixelShader* newPs = nullptr;
	ID3D11InputLayout* newInputLayout = nullptr;

	ID3DBlob* vsBlob = nullptr;
	V_RETURN(CompileShader(L"shaders\\sponza.hlsl", nullptr, "vs_main", SHADER_VERTEX, &vsBlob));
	V_RETURN(pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &newVs));
	DXUT_SetDebugName(newVs, "SponzaVS");

	ID3DBlob* psBlob = nullptr;
	V_RETURN(CompileShader(L"shaders\\sponza.hlsl", nullptr, "ps_main", SHADER_PIXEL, &psBlob));
	V_RETURN(pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &newPs));
	DXUT_SetDebugName(newPs, "SponzaPS");
	SAFE_RELEASE(psBlob);

	const D3D11_INPUT_ELEMENT_DESC elementsDesc[] = {
	    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	    {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 4, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	int iNumElements = sizeof(elementsDesc) / sizeof(D3D11_INPUT_ELEMENT_DESC);

	V_RETURN(pd3dDevice->CreateInputLayout(elementsDesc, iNumElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &newInputLayout));
	SAFE_RELEASE(vsBlob);

	//
	SAFE_RELEASE(m_vs);
	SAFE_RELEASE(m_ps);
	SAFE_RELEASE(m_inputLayout);
	m_vs = newVs;
	m_ps = newPs;
	m_inputLayout = newInputLayout;

	return S_OK;
}
#endif