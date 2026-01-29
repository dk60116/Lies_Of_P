#include "cpch.h"
#include "PlayerCamera.h"

CPlayerCamera::CPlayerCamera()
	: m_pPlayer(nullptr)
	, m_sOptions({})
	, m_fBackOffset(6.f)
	, m_fZoomSensor(0.f)
{
}

CPlayerCamera::~CPlayerCamera()
{
	m_strName = L"PlayerCamera";
}

CPlayerCamera* CPlayerCamera::Create()
{
	return new CPlayerCamera();
}

CComponent* CPlayerCamera::Clone() const
{
	CPlayerCamera* clone = new CPlayerCamera();

	return clone;
}

HRESULT CPlayerCamera::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	m_pGameObject->AddComponent<CCamera>();

	m_fZoomSensor = m_sOptions.firstZoomSensor;

	return S_OK;
}

void CPlayerCamera::Awake()
{
	m_pPlayer = CGameManager::GetInstance().Get_Player();
}

void CPlayerCamera::Start()
{
}

void CPlayerCamera::Update()
{
	_float wheel = CInput::GetInstance().GetAxis(L"Mouse ScrollWheel");

	if (wheel < 0.f)
	{
		m_fBackOffset += m_fZoomSensor * DELTA_TIME;
	}
	else if (wheel > 0.f)
	{
		m_fBackOffset -= m_fZoomSensor * DELTA_TIME;
	}

	m_fBackOffset = std::clamp(m_fBackOffset, m_sOptions.zoomMin, m_sOptions.zoomMax);
}

void CPlayerCamera::LateUpdate()
{
	return;

	if (m_pPlayer)
	{
		CTransform* tf = Get_Transform();
		CTransform* playerTf = m_pPlayer->Get_Transform();
		const vector3 playerPos = playerTf->Get_Position();
		const vector3 forwardOffset = playerTf->Get_Directions().forward * 1.f;
		const vector3 targetPos = playerPos + playerTf->Get_Directions().back * m_fBackOffset + (vector3::up() * m_sOptions.lookHeightOffset * 2.f) + (vector3::down() * (5.f / (m_fBackOffset)));
		tf->Set_Position(vector3::Lerp(tf->Get_Position(), targetPos, m_sOptions.trackingSpeed * DELTA_TIME));
		tf->LookAt(playerPos + vector3::up() * m_sOptions.lookHeightOffset + forwardOffset + vector3::down() * (1.f / (m_fBackOffset * 5.f)) + vector3::down() * (2.5f / m_fBackOffset));
	}
}

void CPlayerCamera::OnDestroy()
{
}