#pragma once

#include "EngineDefine.h"
#include "EngineMacro.h"
#include "EngineTypedef.h"
#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>

NS_BEGIN(Engine)

class ENGINE_DLL CIBLBaker final
{
public:
	CIBLBaker();
	~CIBLBaker();

public:
	HRESULT		Initialize(ID3D11Device* device, ID3D11DeviceContext* ctx);
	HRESULT		LoadSourceCubemap(const wstring& ddsPath);
	HRESULT		BakeAll();
	void		Release();

public:
	ID3D11ShaderResourceView*	GetIrradianceSRV()		const { return m_pIrradianceSRV; }
	ID3D11ShaderResourceView*	GetPrefilteredEnvSRV()	const { return m_pPrefilteredSRV; }
	ID3D11ShaderResourceView*	GetBRDFLutSRV()			const { return m_pBRDFLutSRV; }

private:
	HRESULT		GenerateBRDFLUT();
	HRESULT		GenerateIrradianceMap();
	HRESULT		GeneratePreFilteredEnvMap();

private:
	HRESULT		CompileShader(const wchar_t* path, const char* entry, const char* target, ID3DBlob** outBlob);
	HRESULT		CreateFullscreenQuad();
	void		DrawFullscreenQuad();
	HRESULT		CreateCubemapRT(UINT size, UINT mipLevels, DXGI_FORMAT format,
					ID3D11Texture2D** outTex,
					std::vector<ID3D11RenderTargetView*>& outRTVs,
					ID3D11ShaderResourceView** outSRV);

	void		RenderCubemapFaces(
					ID3D11PixelShader* ps,
					const std::vector<ID3D11RenderTargetView*>& rtvs,
					UINT size, UINT mipLevels,
					bool useRoughness);

private:
	struct BakeCB
	{
		DirectX::XMFLOAT4X4	invFaceViewProj;
		float				roughness;
		float				pad[3];
	};

private:
	ID3D11Device*				m_pDevice;
	ID3D11DeviceContext*		m_pContext;

	// Source
	ID3D11ShaderResourceView*	m_pSourceEnvSRV;

	// Outputs
	ID3D11Texture2D*			m_pIrradianceTex;
	ID3D11ShaderResourceView*	m_pIrradianceSRV;

	ID3D11Texture2D*			m_pPrefilteredTex;
	ID3D11ShaderResourceView*	m_pPrefilteredSRV;

	ID3D11Texture2D*			m_pBRDFLutTex;
	ID3D11ShaderResourceView*	m_pBRDFLutSRV;

	// Bake internals
	ID3D11VertexShader*			m_pBakeVS;
	ID3D11InputLayout*			m_pBakeIL;
	ID3D11PixelShader*			m_pBRDFIntegrationPS;
	ID3D11PixelShader*			m_pIrradiancePS;
	ID3D11PixelShader*			m_pPrefilterPS;

	ID3D11Buffer*				m_pQuadVB;
	ID3D11Buffer*				m_pQuadIB;
	ID3D11Buffer*				m_pBakeCB;

	ID3D11SamplerState*			m_pLinearClampSampler;

	static const UINT IRRADIANCE_SIZE		= 32;
	static const UINT PREFILTER_SIZE		= 128;
	static const UINT PREFILTER_MIP_LEVELS	= 5;
	static const UINT BRDF_LUT_SIZE		= 256;
};

NS_END
