#include "epch.h"
#include "Shader.h"

CShader::CShader()
	: m_sInfo({})
	, m_pVertexShader(nullptr)
	, m_pPixelShader(nullptr)
	, m_pInputLayout(nullptr)
{
	m_strName = L"Shader";
}

CShader::~CShader()
{
	OnDestroy();
}

CShader* CShader::Create(const wstring _path)
{
	return new CShader();
}

ID3D11VertexShader* CShader::Get_VertexShader() const
{
	return m_pVertexShader;
}

ID3D11PixelShader* CShader::Get_PixelShader() const
{
	return m_pPixelShader;
}

ID3D11InputLayout* CShader::Get_InputLayout() const
{
	return m_pInputLayout;
}

HRESULT CShader::Initialize(const wstring& _name, const wstring& _filePath, void* _desc)
{
	if (FAILED(__super::Initialize(_name, _filePath, _desc)))
		return E_FAIL;

	SHADERDESC* info = reinterpret_cast<SHADERDESC*>(_desc);

	if (!info)
		return E_FAIL;

	m_sInfo.vsPath = info->vsPath;
	m_sInfo.psPath = info->psPath;
	m_sInfo.layoutCount = info->layoutCount;
	m_sInfo.layout = info->layout;

	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

	if (!device || !context)
		return E_FAIL;

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> vsBlob = nullptr;
	ComPtr<ID3DBlob> psBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3DCompileFromFile(m_sInfo.vsPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
			CDebug::LogError((char*)errorBlob->GetBufferPointer());
		return hr;
	}

	hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_pVertexShader);
	if (FAILED(hr))
	{
		if (errorBlob)
			CDebug::LogError((char*)errorBlob->GetBufferPointer());
		return hr;
	}

	hr = D3DCompileFromFile((m_sInfo.psPath.length() > 0) ? m_sInfo.psPath.c_str() : m_sInfo.vsPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PSMain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
	if (FAILED(hr))
	{
		if (errorBlob)
			CDebug::LogError((char*)errorBlob->GetBufferPointer());
		return hr;
	}

	hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pPixelShader);
	if (FAILED(hr))
		return hr;

	hr = device->CreateInputLayout(m_sInfo.layout, m_sInfo.layoutCount,
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_pInputLayout);

	if (FAILED(hr))
		return hr;

	return S_OK;
}

void CShader::OnDestroy()
{
	Safe_Release(m_pVertexShader);
	Safe_Release(m_pPixelShader);
	Safe_Release(m_pInputLayout);
}

void CShader::Bind() const
{
	auto* context = CGraphicDevice::GetInstance().Get_Context();

	if (context && m_pVertexShader && m_pPixelShader && m_pInputLayout)
	{
		context->IASetInputLayout(m_pInputLayout);
		context->VSSetShader(m_pVertexShader, nullptr, 0);
		context->PSSetShader(m_pPixelShader, nullptr, 0);
	}
}
