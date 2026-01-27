#pragma once

#include "EngineResource.h"

NS_BEGIN(Engine)

class ENGINE_DLL CShader : public CEngineResource
{
	friend class CResources;
	friend class CMaterial;

public:
	typedef struct ShaderInfoDescription
	{
		wstring vsPath = L"";
		wstring psPath = L"";
		_uint layoutCount = 0;
		const D3D11_INPUT_ELEMENT_DESC* layout;
	} SHADERDESC;

private:
	explicit CShader();
	virtual ~CShader();

public:
	static CShader* Create(const wstring _path = L"");

private:
	HRESULT Initialize(const wstring& _name, const wstring& _filePath, void* _desc) override;
	void OnDestroy() override;
	void Bind() const;

public:
	ID3D11VertexShader* Get_VertexShader() const;
	ID3D11PixelShader* Get_PixelShader() const;
	ID3D11InputLayout* Get_InputLayout() const;

private:
	SHADERDESC m_sInfo;

	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader* m_pPixelShader;
	ID3D11InputLayout* m_pInputLayout;
};

NS_END

