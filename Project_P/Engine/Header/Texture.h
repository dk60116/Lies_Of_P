#pragma once

#include "EngineResource.h"

NS_BEGIN(Engine)

class ENGINE_DLL CTexture : public CEngineResource
{
	friend class CResources;

private:
	explicit CTexture();
	~CTexture();

private:
	static CTexture* Create();
	HRESULT Initialize(const wstring& _name, const wstring& _filePath, void* _desc) override;
	void OnDestroy() override;

public:
	ID3D11Texture2D* Get_Texture() const;
	ID3D11ShaderResourceView* Get_SRV() const;

	const D3D11_TEXTURE2D_DESC& Get_TextureDesc();

private:
	ID3D11Texture2D* m_pTexture;
	ID3D11ShaderResourceView* m_pSRV;

	D3D11_TEXTURE2D_DESC m_sTextureDesc;
};

NS_END

