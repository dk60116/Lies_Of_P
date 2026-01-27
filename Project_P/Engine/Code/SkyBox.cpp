#include "epch.h"
#include "SkyBox.h"

CSkyBox::CSkyBox()
	: m_pMaterial(nullptr)
	, m_sSaveDesc({})
{
}

CSkyBox::~CSkyBox()
{
	OnDestroy();
}

HRESULT CSkyBox::Initialize(const wstring& _name, const wstring& _filePath, void* _desc)
{
	if (FAILED(__super::Initialize(_name, L"../Assets/Cube", nullptr)))
		return E_FAIL;

	if (_desc)
		m_sSaveDesc = *static_cast<SKYBOXBUFFERDESC*>(_desc);

	return S_OK;
}

HRESULT CSkyBox::Initialize_Scene()
{
	m_pMaterial = CResources::GetInstance().CloneOnGame<CMaterial>(L"SkyBoxMaterial (Material)");

	if (!m_pMaterial)
		return E_FAIL;

	m_pMaterial->Set_Texture(CResources::GetInstance().LoadOnGame<CTexture>(m_sSaveDesc.texture));

	if (!m_pMaterial->Get_Texture(0))
		m_pMaterial->Set_Texture(CResources::GetInstance().LoadOnScene<CTexture>(m_sSaveDesc.texture));

	return S_OK;
}

CSkyBox* CSkyBox::Create()
{
	return new CSkyBox();
}

void CSkyBox::OnDestroy()
{
}

void CSkyBox::RenderSky(CCamera* _camera)
{
	if (!m_pMaterial)
	{
		//CDebug::LogError("Sky box rneder failure: No material");
		return;
	}
	if (!m_pMaterial) return;

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	
	_matrix viewMat = _camera->Get_ViewMatrix();

	_float4x4 viewFloat4x4;
	XMStoreFloat4x4(&viewFloat4x4, viewMat);

	viewFloat4x4._41 = 0.0f;
	viewFloat4x4._42 = 0.0f;
	viewFloat4x4._43 = 0.0f;

	_matrix viewNoTrans = XMLoadFloat4x4(&viewFloat4x4);

	m_pMaterial->Bind_Matrix(XMMatrixIdentity());
	m_pMaterial->Bind_Camera
	(
		_camera->Get_Transform()->Get_Position(),
		viewNoTrans,
		_camera->Get_ProjectionMatrix()
	);

	__super::Render(); 
}
