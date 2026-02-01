#include "cpch.h"
#include "PlayerController.h"
#include "Player.h"

CPlayerController::CPlayerController()
	: m_pPlayer(nullptr)
	, m_pPlayerCam(nullptr)
	, m_bRunning(nullptr)
	, m_mKeyHold({})
	, m_vMoveDirection({})
	, m_fTargetYaw(0.f)
{
}

CPlayerController::~CPlayerController()
{
}

CPlayerController* CPlayerController::Create()
{
	return new CPlayerController();
}

CComponent* CPlayerController::Clone() const
{
	CPlayerController* clone = new CPlayerController();
	return clone;
}

HRESULT CPlayerController::Initialize()
{
	m_mKeyHold.insert({ Forward, false });
	m_mKeyHold.insert({ Back, false });
	m_mKeyHold.insert({ Left, false });
	m_mKeyHold.insert({ Right, false });

	return S_OK;
}

void CPlayerController::Awake()
{
	m_pPlayerCam = CGameManager::GetInstance().Get_PlayerCamera();
}

void CPlayerController::Start()
{
}

void CPlayerController::Update()
{
	Update_Key();
	Update_Move();
}

void CPlayerController::LateUpdate()
{
}

void CPlayerController::OnDestroy()
{
}

void CPlayerController::Set_Player(CPlayer* _player)
{
	m_pPlayer = _player;
}

void CPlayerController::Set_Camera(CPlayerCamera* _cam)
{
	m_pPlayerCam = _cam;
}

const _bool CPlayerController::IsRunning() const
{
	return m_bRunning;
}

void CPlayerController::Update_Key()
{
	m_mKeyHold[Forward] = CInput::GetInstance().GetKey(W);
	m_mKeyHold[Back] = CInput::GetInstance().GetKey(S);
	m_mKeyHold[Left] = CInput::GetInstance().GetKey(A);
	m_mKeyHold[Right] = CInput::GetInstance().GetKey(D);
}

void CPlayerController::Update_Move()
{
	m_bRunning = m_mKeyHold[Forward] || m_mKeyHold[Back] || m_mKeyHold[Left] || m_mKeyHold[Right];

	auto tr = m_pPlayer->Get_Transform();

	vector3 camForward = m_pPlayerCam->Get_ForwardVector();
	m_fTargetYaw = m_pPlayerCam->Get_ForwardAngle();

	auto& status = m_pPlayer->Get_PlayerStatus();

	vector3 e;

	if (m_bRunning)
	{
		m_pPlayer->Get_Transform()->Add_Position(camForward * status.moveSpeed * DELTA_TIME);
		e = tr->Get_EulerAngles();
		m_fCrtYaw = e.y;

		m_bRotate = true;
	}

	if (m_bRotate && abs(m_fCrtYaw - m_fTargetYaw) > 0.1f)
		m_fCrtYaw = Lerp(m_fCrtYaw, m_fTargetYaw, status.turnSpeed * DELTA_TIME);
	else
		m_bRotate = false;

	tr->Set_EulerAngles(e.x, m_fCrtYaw, e.z);
}
