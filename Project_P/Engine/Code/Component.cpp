#include "epch.h"
#include "Component.h"
#include "GameObject.h"

CComponent::CComponent()
	: m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_bEnable(true)
	, m_bSaveTarget(true)
	, m_pGameObject(nullptr)
	, m_iSortIndex(0)
{
}

CComponent::~CComponent()
{
	Release();
}

HRESULT CComponent::Initialize()
{
	m_pDevice = CGraphicDevice::GetInstance().Get_Device();
	m_pContext = CGraphicDevice::GetInstance().Get_Context();

	m_pDevice->AddRef();
	m_pContext->AddRef();

	return S_OK;
}

void CComponent::ComponentRelease()
{
	Safe_Release(m_pDevice);
	Safe_Release(m_pContext);
}

void CComponent::Awake()
{
}

void CComponent::Start()
{
}

void CComponent::Update_Editor()
{
}

void CComponent::Update()
{
}

void CComponent::FixedUpdate()
{
}

void CComponent::LateUpdate_Editor()
{
}

void CComponent::LateUpdate()
{
}

void CComponent::OnMouseEnter()
{
}

void CComponent::OnMouseOver()
{
}

void CComponent::OnMouseExit()
{
}

void CComponent::OnMouseDown()
{
}

void CComponent::OnMouseDrag()
{
}

void CComponent::OnMouseUp()
{
}

void CComponent::OnCollisionEnter(CCollider* _other)
{
}

void CComponent::OnCollisionStay(CCollider* _other)
{
}

void CComponent::OnCollisionExit(CCollider* _other)
{
}

void CComponent::OnTriggerEnter(CCollider* _other)
{
}

void CComponent::OnTriggerStay(CCollider* _other)
{
}

void CComponent::OnTriggerExit(CCollider* _other)
{
}

void CComponent::OnPreCull_Editor()
{
}

void CComponent::OnPreRender_Editor()
{
}

void CComponent::Render_Editor()
{
}

void CComponent::OnPostRender_Editor()
{
}

void CComponent::OnPreCull()
{
}

void CComponent::OnPreRender()
{
}

void CComponent::Render()
{
}

void CComponent::OnPostRender()
{
}

void CComponent::Render_Gizmo()
{
}

void CComponent::OnEnable()
{
}

void CComponent::OnDisable()
{
}

void CComponent::OnApplicationQuit()
{
}

const _bool CComponent::Get_Enable() const
{
	return m_bEnable;
}

void CComponent::SetEnable(const _bool _enable)
{
	if (m_bEnable == _enable)
		return;

	const _bool wasEnabled = m_bEnable;
	m_bEnable = _enable;

	if (!m_pGameObject || !m_pGameObject->IsRecursiveActive())
		return;

	if (wasEnabled)
		OnDisable();
	else
		OnEnable();
}


CGameObject* CComponent::Get_GameObject()
{
	return m_pGameObject;
}

void CComponent::Set_Object(CGameObject* _gameObject)
{
	m_pGameObject = _gameObject;
}

vector<FieldInfo> CComponent::GetInspectorFields()
{
	return vector<FieldInfo> {};
}

CTransform* CComponent::GetTransform()
{
	return m_pGameObject->GetTransform();
}

const _int CComponent::GetSortIndex() const
{
	return m_iSortIndex;
}

const _bool CComponent::Is_SaveTarget() const
{
	return m_bSaveTarget;
}



