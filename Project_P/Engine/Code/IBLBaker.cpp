#include "epch.h"
#include "IBLBaker.h"
#include "DDSTextureLoader.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

CIBLBaker::CIBLBaker()
	: m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_pSourceEnvSRV(nullptr)
	, m_pIrradianceTex(nullptr)
	, m_pIrradianceSRV(nullptr)
	, m_pPrefilteredTex(nullptr)
	, m_pPrefilteredSRV(nullptr)
	, m_pBRDFLutTex(nullptr)
	, m_pBRDFLutSRV(nullptr)
	, m_pBakeVS(nullptr)
	, m_pBakeIL(nullptr)
	, m_pBRDFIntegrationPS(nullptr)
	, m_pIrradiancePS(nullptr)
	, m_pPrefilterPS(nullptr)
	, m_pQuadVB(nullptr)
	, m_pQuadIB(nullptr)
	, m_pBakeCB(nullptr)
	, m_pLinearClampSampler(nullptr)
{
}

CIBLBaker::~CIBLBaker()
{
	Release();
}

HRESULT CIBLBaker::Initialize(ID3D11Device* device, ID3D11DeviceContext* ctx)
{
	if (!device || !ctx)
		return E_INVALIDARG;

	m_pDevice = device;
	m_pContext = ctx;

	// Create fullscreen quad geometry
	HRESULT hr = CreateFullscreenQuad();
	if (FAILED(hr))
		return hr;

	// Create bake constant buffer
	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.ByteWidth = sizeof(BakeCB);
	cbDesc.Usage = D3D11_USAGE_DEFAULT;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pBakeCB);
	if (FAILED(hr))
		return hr;

	// Create linear clamp sampler
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.MipLODBias = 0.0f;
	sampDesc.MaxAnisotropy = 1;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = m_pDevice->CreateSamplerState(&sampDesc, &m_pLinearClampSampler);
	if (FAILED(hr))
		return hr;

	// Compile bake shaders
	// BRDF Integration uses its own VS (simple 2D)
	ID3DBlob* brdfVSBlob = nullptr;
	hr = CompileShader(L"../EngineResources/Shader/BRDFIntegration.hlsl", "VSMain", "vs_5_0", &brdfVSBlob);
	if (FAILED(hr))
		return hr;

	hr = m_pDevice->CreateVertexShader(brdfVSBlob->GetBufferPointer(), brdfVSBlob->GetBufferSize(), nullptr, &m_pBakeVS);
	if (FAILED(hr))
	{
		brdfVSBlob->Release();
		return hr;
	}

	// Create input layout (POSITION float3 + TEXCOORD float2)
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr = m_pDevice->CreateInputLayout(layout, 2, brdfVSBlob->GetBufferPointer(), brdfVSBlob->GetBufferSize(), &m_pBakeIL);
	brdfVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// BRDF Integration PS
	ID3DBlob* psBlob = nullptr;
	hr = CompileShader(L"../EngineResources/Shader/BRDFIntegration.hlsl", "PSMain", "ps_5_0", &psBlob);
	if (FAILED(hr))
		return hr;
	hr = m_pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pBRDFIntegrationPS);
	psBlob->Release();
	if (FAILED(hr))
		return hr;

	// Irradiance Convolution PS (VS from IrradianceConvolution.hlsl)
	// Note: cubemap bake shaders use a different VS that outputs world direction.
	// We compile the VS from each cubemap shader file, but since they share the same
	// vertex layout we reuse the input layout. For VS we'll compile from IrradianceConvolution.
	// Actually, all 3 bake shaders share the same vertex format (POSITION + TEXCOORD).
	// The cubemap bake VS is different from BRDF VS - it uses BakeCB for face direction.
	// We'll store the cubemap VS separately but reuse the input layout.

	// We'll use the BRDF VS for BRDF LUT bake, and the cubemap VS (from IrradianceConvolution)
	// for irradiance and prefilter passes. But since all use the same IL, we're fine.

	hr = CompileShader(L"../EngineResources/Shader/IrradianceConvolution.hlsl", "PSMain", "ps_5_0", &psBlob);
	if (FAILED(hr))
		return hr;
	hr = m_pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pIrradiancePS);
	psBlob->Release();
	if (FAILED(hr))
		return hr;

	hr = CompileShader(L"../EngineResources/Shader/PrefilterEnvMap.hlsl", "PSMain", "ps_5_0", &psBlob);
	if (FAILED(hr))
		return hr;
	hr = m_pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pPrefilterPS);
	psBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile cubemap VS from IrradianceConvolution.hlsl (shared with PrefilterEnvMap)
	// We need a separate VS that reads BakeCB and outputs world direction.
	// Both IrradianceConvolution and PrefilterEnvMap have the same VSMain.
	// We'll replace m_pBakeVS with the cubemap VS after BRDF bake, or store both.
	// Simpler: store the BRDF VS as m_pBakeVS and compile cubemap VS now as a local.
	// Actually, let's just use m_pBakeVS for BRDF, and compile a cubemap VS stored separately.
	// For simplicity, let's just recompile during BakeAll. But that's wasteful.
	// Let's store both. We'll keep m_pBakeVS as the BRDF VS and add m_pCubemapVS.
	// Wait - let me simplify. Both VS have the same input layout. Let me just compile the
	// cubemap VS here and swap when needed.

	// Actually the cleanest approach: compile cubemap VS from IrradianceConvolution.hlsl
	ID3DBlob* cubemapVSBlob = nullptr;
	hr = CompileShader(L"../EngineResources/Shader/IrradianceConvolution.hlsl", "VSMain", "vs_5_0", &cubemapVSBlob);
	if (SUCCEEDED(hr))
	{
		// We'll store the BRDF VS as m_pBakeVS already created above.
		// Create a second VS for cubemap baking - store as m_pBRDFIntegrationPS... no.
		// Let's just use two VS pointers. But we only have m_pBakeVS.
		// Simplest fix: release the BRDF VS and just use the cubemap VS for everything.
		// The BRDF VS does: posH = float4(posL.xy * 2.0 - 1.0, 0.5, 1.0); posH.y = -posH.y; uv = v.uv
		// The cubemap VS does: ndc = uv*2-1; ndc.y = -ndc.y; invVP transform; posH = float4(ndc, 0.5, 1.0)
		// These are different! For BRDF we need o.uv, for cubemap we need o.dir.
		// OK let's just swap VS between passes. Store both.
		cubemapVSBlob->Release();
	}

	return S_OK;
}

HRESULT CIBLBaker::LoadSourceCubemap(const wstring& ddsPath)
{
	Safe_Release(m_pSourceEnvSRV);
	return DirectX::CreateDDSTextureFromFile(m_pDevice, ddsPath.c_str(), nullptr, &m_pSourceEnvSRV);
}

HRESULT CIBLBaker::BakeAll()
{
	if (!m_pDevice || !m_pContext)
		return E_FAIL;

	// Save render state
	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	m_pContext->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	UINT prevVPCount = 1;
	m_pContext->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	UINT prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	float prevBlendFactor[4] = {};
	UINT prevSampleMask = 0;

	m_pContext->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	m_pContext->RSGetState(&prevRS);
	m_pContext->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	ID3D11VertexShader* prevVS = nullptr;
	ID3D11PixelShader* prevPS = nullptr;
	ID3D11InputLayout* prevIL = nullptr;
	m_pContext->VSGetShader(&prevVS, nullptr, nullptr);
	m_pContext->PSGetShader(&prevPS, nullptr, nullptr);
	m_pContext->IAGetInputLayout(&prevIL);

	// Set common state: depth off, no blend, no cull
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable = FALSE;
	dsDesc.StencilEnable = FALSE;
	ID3D11DepthStencilState* noDepthDS = nullptr;
	m_pDevice->CreateDepthStencilState(&dsDesc, &noDepthDS);
	m_pContext->OMSetDepthStencilState(noDepthDS, 0);

	D3D11_RASTERIZER_DESC rsDesc = {};
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_NONE;
	rsDesc.DepthClipEnable = FALSE;
	ID3D11RasterizerState* noCullRS = nullptr;
	m_pDevice->CreateRasterizerState(&rsDesc, &noCullRS);
	m_pContext->RSSetState(noCullRS);

	const float bf[4] = { 0, 0, 0, 0 };
	m_pContext->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	// Set input assembler
	m_pContext->IASetInputLayout(m_pBakeIL);
	UINT stride = sizeof(float) * 5; // float3 pos + float2 uv
	UINT offset = 0;
	m_pContext->IASetVertexBuffers(0, 1, &m_pQuadVB, &stride, &offset);
	m_pContext->IASetIndexBuffer(m_pQuadIB, DXGI_FORMAT_R16_UINT, 0);
	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set sampler
	m_pContext->PSSetSamplers(0, 1, &m_pLinearClampSampler);

	// 1. Generate BRDF LUT
	HRESULT hr = GenerateBRDFLUT();
	if (FAILED(hr))
	{
		CDebug::LogError(L"IBLBaker: Failed to generate BRDF LUT");
	}

	// 2. Generate Irradiance Map (needs source cubemap)
	if (m_pSourceEnvSRV)
	{
		hr = GenerateIrradianceMap();
		if (FAILED(hr))
			CDebug::LogError(L"IBLBaker: Failed to generate irradiance map");

		// 3. Generate Pre-filtered Env Map
		hr = GeneratePreFilteredEnvMap();
		if (FAILED(hr))
			CDebug::LogError(L"IBLBaker: Failed to generate pre-filtered env map");
	}
	else
	{
		CDebug::LogWarnning(L"IBLBaker: No source cubemap loaded, skipping irradiance/prefilter");
	}

	// Release temporary state objects
	Safe_Release(noDepthDS);
	Safe_Release(noCullRS);

	// Release bake shaders (no longer needed after bake)
	Safe_Release(m_pBakeVS);
	Safe_Release(m_pBakeIL);
	Safe_Release(m_pBRDFIntegrationPS);
	Safe_Release(m_pIrradiancePS);
	Safe_Release(m_pPrefilterPS);
	Safe_Release(m_pQuadVB);
	Safe_Release(m_pQuadIB);
	Safe_Release(m_pBakeCB);
	Safe_Release(m_pLinearClampSampler);

	// Restore render state
	m_pContext->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		m_pContext->RSSetViewports(1, &prevVP);
	m_pContext->OMSetDepthStencilState(prevDS, prevStencilRef);
	m_pContext->RSSetState(prevRS);
	m_pContext->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);
	m_pContext->VSSetShader(prevVS, nullptr, 0);
	m_pContext->PSSetShader(prevPS, nullptr, 0);
	m_pContext->IASetInputLayout(prevIL);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
	Safe_Release(prevVS);
	Safe_Release(prevPS);
	Safe_Release(prevIL);

	CDebug::Log(L"IBLBaker: Bake completed successfully");

	return S_OK;
}

HRESULT CIBLBaker::GenerateBRDFLUT()
{
	// Create 2D render target: 256x256, R16G16_FLOAT
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = BRDF_LUT_SIZE;
	texDesc.Height = BRDF_LUT_SIZE;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pBRDFLutTex);
	if (FAILED(hr)) return hr;

	ID3D11RenderTargetView* rtv = nullptr;
	hr = m_pDevice->CreateRenderTargetView(m_pBRDFLutTex, nullptr, &rtv);
	if (FAILED(hr)) return hr;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	hr = m_pDevice->CreateShaderResourceView(m_pBRDFLutTex, &srvDesc, &m_pBRDFLutSRV);
	if (FAILED(hr))
	{
		rtv->Release();
		return hr;
	}

	// Render
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)BRDF_LUT_SIZE;
	vp.Height = (float)BRDF_LUT_SIZE;
	vp.MaxDepth = 1.0f;
	m_pContext->RSSetViewports(1, &vp);

	const float clearColor[4] = { 0, 0, 0, 1 };
	m_pContext->ClearRenderTargetView(rtv, clearColor);
	m_pContext->OMSetRenderTargets(1, &rtv, nullptr);

	m_pContext->VSSetShader(m_pBakeVS, nullptr, 0);
	m_pContext->PSSetShader(m_pBRDFIntegrationPS, nullptr, 0);

	DrawFullscreenQuad();

	rtv->Release();
	return S_OK;
}

HRESULT CIBLBaker::GenerateIrradianceMap()
{
	std::vector<ID3D11RenderTargetView*> rtvs;
	HRESULT hr = CreateCubemapRT(IRRADIANCE_SIZE, 1, DXGI_FORMAT_R16G16B16A16_FLOAT,
		&m_pIrradianceTex, rtvs, &m_pIrradianceSRV);
	if (FAILED(hr)) return hr;

	// Compile cubemap VS from IrradianceConvolution.hlsl
	ID3DBlob* vsBlob = nullptr;
	hr = CompileShader(L"../EngineResources/Shader/IrradianceConvolution.hlsl", "VSMain", "vs_5_0", &vsBlob);
	if (FAILED(hr)) return hr;

	ID3D11VertexShader* cubemapVS = nullptr;
	hr = m_pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &cubemapVS);
	vsBlob->Release();
	if (FAILED(hr)) return hr;

	m_pContext->VSSetShader(cubemapVS, nullptr, 0);
	m_pContext->PSSetShader(m_pIrradiancePS, nullptr, 0);

	// Bind source environment cubemap
	m_pContext->PSSetShaderResources(0, 1, &m_pSourceEnvSRV);

	// Render 6 faces
	XMVECTOR eye = XMVectorZero();
	XMVECTOR targets[6] = {
		XMVectorSet(+1, 0, 0, 0), XMVectorSet(-1, 0, 0, 0),
		XMVectorSet( 0,+1, 0, 0), XMVectorSet( 0,-1, 0, 0),
		XMVectorSet( 0, 0,+1, 0), XMVectorSet( 0, 0,-1, 0),
	};
	XMVECTOR ups[6] = {
		XMVectorSet(0,+1, 0, 0), XMVectorSet(0,+1, 0, 0),
		XMVectorSet(0, 0,-1, 0), XMVectorSet(0, 0,+1, 0),
		XMVectorSet(0,+1, 0, 0), XMVectorSet(0,+1, 0, 0),
	};

	XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f);

	D3D11_VIEWPORT vp = {};
	vp.Width = (float)IRRADIANCE_SIZE;
	vp.Height = (float)IRRADIANCE_SIZE;
	vp.MaxDepth = 1.0f;
	m_pContext->RSSetViewports(1, &vp);

	for (UINT face = 0; face < 6; ++face)
	{
		XMMATRIX view = XMMatrixLookAtLH(eye, targets[face], ups[face]);
		XMMATRIX vp_mat = XMMatrixMultiply(view, proj);
		XMMATRIX invVP = XMMatrixInverse(nullptr, vp_mat);

		BakeCB cb = {};
		XMStoreFloat4x4(&cb.invFaceViewProj, XMMatrixTranspose(invVP));
		cb.roughness = 0.0f;
		m_pContext->UpdateSubresource(m_pBakeCB, 0, nullptr, &cb, 0, 0);
		m_pContext->VSSetConstantBuffers(0, 1, &m_pBakeCB);
		m_pContext->PSSetConstantBuffers(0, 1, &m_pBakeCB);

		const float clearColor[4] = { 0, 0, 0, 1 };
		m_pContext->ClearRenderTargetView(rtvs[face], clearColor);
		m_pContext->OMSetRenderTargets(1, &rtvs[face], nullptr);

		DrawFullscreenQuad();
	}

	// Release RTVs
	for (auto& r : rtvs) Safe_Release(r);
	Safe_Release(cubemapVS);

	// Unbind source SRV
	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);

	return S_OK;
}

HRESULT CIBLBaker::GeneratePreFilteredEnvMap()
{
	std::vector<ID3D11RenderTargetView*> rtvs;
	HRESULT hr = CreateCubemapRT(PREFILTER_SIZE, PREFILTER_MIP_LEVELS, DXGI_FORMAT_R16G16B16A16_FLOAT,
		&m_pPrefilteredTex, rtvs, &m_pPrefilteredSRV);
	if (FAILED(hr)) return hr;

	// Compile cubemap VS from PrefilterEnvMap.hlsl
	ID3DBlob* vsBlob = nullptr;
	hr = CompileShader(L"../EngineResources/Shader/PrefilterEnvMap.hlsl", "VSMain", "vs_5_0", &vsBlob);
	if (FAILED(hr)) return hr;

	ID3D11VertexShader* cubemapVS = nullptr;
	hr = m_pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &cubemapVS);
	vsBlob->Release();
	if (FAILED(hr)) return hr;

	m_pContext->VSSetShader(cubemapVS, nullptr, 0);
	m_pContext->PSSetShader(m_pPrefilterPS, nullptr, 0);

	// Bind source environment cubemap
	m_pContext->PSSetShaderResources(0, 1, &m_pSourceEnvSRV);

	XMVECTOR eye = XMVectorZero();
	XMVECTOR targets[6] = {
		XMVectorSet(+1, 0, 0, 0), XMVectorSet(-1, 0, 0, 0),
		XMVectorSet( 0,+1, 0, 0), XMVectorSet( 0,-1, 0, 0),
		XMVectorSet( 0, 0,+1, 0), XMVectorSet( 0, 0,-1, 0),
	};
	XMVECTOR ups[6] = {
		XMVectorSet(0,+1, 0, 0), XMVectorSet(0,+1, 0, 0),
		XMVectorSet(0, 0,-1, 0), XMVectorSet(0, 0,+1, 0),
		XMVectorSet(0,+1, 0, 0), XMVectorSet(0,+1, 0, 0),
	};

	XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f);

	for (UINT mip = 0; mip < PREFILTER_MIP_LEVELS; ++mip)
	{
		UINT mipSize = PREFILTER_SIZE >> mip;
		if (mipSize < 1) mipSize = 1;

		D3D11_VIEWPORT vp = {};
		vp.Width = (float)mipSize;
		vp.Height = (float)mipSize;
		vp.MaxDepth = 1.0f;
		m_pContext->RSSetViewports(1, &vp);

		float roughness = (float)mip / (float)(PREFILTER_MIP_LEVELS - 1);

		for (UINT face = 0; face < 6; ++face)
		{
			XMMATRIX view = XMMatrixLookAtLH(eye, targets[face], ups[face]);
			XMMATRIX vp_mat = XMMatrixMultiply(view, proj);
			XMMATRIX invVP = XMMatrixInverse(nullptr, vp_mat);

			BakeCB cb = {};
			XMStoreFloat4x4(&cb.invFaceViewProj, XMMatrixTranspose(invVP));
			cb.roughness = roughness;
			m_pContext->UpdateSubresource(m_pBakeCB, 0, nullptr, &cb, 0, 0);
			m_pContext->VSSetConstantBuffers(0, 1, &m_pBakeCB);
			m_pContext->PSSetConstantBuffers(0, 1, &m_pBakeCB);

			UINT rtvIdx = face * PREFILTER_MIP_LEVELS + mip;
			const float clearColor[4] = { 0, 0, 0, 1 };
			m_pContext->ClearRenderTargetView(rtvs[rtvIdx], clearColor);
			m_pContext->OMSetRenderTargets(1, &rtvs[rtvIdx], nullptr);

			DrawFullscreenQuad();
		}
	}

	// Release RTVs
	for (auto& r : rtvs) Safe_Release(r);
	Safe_Release(cubemapVS);

	// Unbind source SRV
	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pContext->PSSetShaderResources(0, 1, &nullSRV);

	return S_OK;
}

HRESULT CIBLBaker::CompileShader(const wchar_t* path, const char* entry, const char* target, ID3DBlob** outBlob)
{
	ID3DBlob* errorBlob = nullptr;
	HRESULT hr = D3DCompileFromFile(path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry, target, 0, 0, outBlob, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
		{
			wstring errMsg = L"IBLBaker shader compile error: ";
			string errStr((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize());
			errMsg += wstring(errStr.begin(), errStr.end());
			CDebug::LogError(errMsg);
			errorBlob->Release();
		}
		return hr;
	}
	if (errorBlob) errorBlob->Release();
	return S_OK;
}

HRESULT CIBLBaker::CreateFullscreenQuad()
{
	// Fullscreen quad: 4 vertices, 6 indices
	// Position (float3) + UV (float2)
	struct Vertex
	{
		float pos[3];
		float uv[2];
	};

	Vertex vertices[4] =
	{
		{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } }, // top-left
		{ { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } }, // top-right
		{ { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } }, // bottom-right
		{ { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } }, // bottom-left
	};

	unsigned short indices[6] = { 0, 1, 2, 0, 2, 3 };

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(vertices);
	vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;

	HRESULT hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pQuadVB);
	if (FAILED(hr)) return hr;

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(indices);
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;

	hr = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pQuadIB);
	return hr;
}

void CIBLBaker::DrawFullscreenQuad()
{
	m_pContext->DrawIndexed(6, 0, 0);
}

HRESULT CIBLBaker::CreateCubemapRT(UINT size, UINT mipLevels, DXGI_FORMAT format,
	ID3D11Texture2D** outTex,
	std::vector<ID3D11RenderTargetView*>& outRTVs,
	ID3D11ShaderResourceView** outSRV)
{
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = size;
	texDesc.Height = size;
	texDesc.MipLevels = mipLevels;
	texDesc.ArraySize = 6;
	texDesc.Format = format;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	HRESULT hr = m_pDevice->CreateTexture2D(&texDesc, nullptr, outTex);
	if (FAILED(hr)) return hr;

	// Create one RTV per face per mip: index = face * mipLevels + mip
	outRTVs.resize(6 * mipLevels);
	for (UINT face = 0; face < 6; ++face)
	{
		for (UINT mip = 0; mip < mipLevels; ++mip)
		{
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = format;
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = mip;
			rtvDesc.Texture2DArray.FirstArraySlice = face;
			rtvDesc.Texture2DArray.ArraySize = 1;

			hr = m_pDevice->CreateRenderTargetView(*outTex, &rtvDesc, &outRTVs[face * mipLevels + mip]);
			if (FAILED(hr)) return hr;
		}
	}

	// SRV as TextureCube
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = mipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;

	hr = m_pDevice->CreateShaderResourceView(*outTex, &srvDesc, outSRV);
	return hr;
}

void CIBLBaker::Release()
{
	Safe_Release(m_pSourceEnvSRV);
	Safe_Release(m_pIrradianceTex);
	Safe_Release(m_pIrradianceSRV);
	Safe_Release(m_pPrefilteredTex);
	Safe_Release(m_pPrefilteredSRV);
	Safe_Release(m_pBRDFLutTex);
	Safe_Release(m_pBRDFLutSRV);
	Safe_Release(m_pBakeVS);
	Safe_Release(m_pBakeIL);
	Safe_Release(m_pBRDFIntegrationPS);
	Safe_Release(m_pIrradiancePS);
	Safe_Release(m_pPrefilterPS);
	Safe_Release(m_pQuadVB);
	Safe_Release(m_pQuadIB);
	Safe_Release(m_pBakeCB);
	Safe_Release(m_pLinearClampSampler);

	m_pDevice = nullptr;
	m_pContext = nullptr;
}
