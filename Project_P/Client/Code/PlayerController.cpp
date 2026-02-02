#include "cpch.h"
#include "PlayerController.h"
#include "Player.h"

#include "PlayerState_Locomotion.h"
#include "PlayerState_Idle.h"
#include "PlayerState_Move.h"

CPlayerController::CPlayerController()
	: m_pPlayer(nullptr)
	, m_pPlayerCam(nullptr)
	, m_bFSMStarted(false)
	, m_bRunning(false)
	, m_mStateList({})
	, m_pRoot(nullptr)
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

	m_mStateList.insert({ PlayerState::Locomotion, new CPlayerState_Locomotion() });
	m_mStateList.insert({ PlayerState::Idle, new CPlayerState_Idle() });
	m_mStateList.insert({ PlayerState::Move, new CPlayerState_Move() });

	auto loco = static_cast<CPlayerState_Locomotion*>(Get_PlayerState(PlayerState::Locomotion));
	loco->SetChildren
	(
		Get_PlayerState(PlayerState::Idle),
		Get_PlayerState(PlayerState::Move)
	);

	m_pRoot = Get_PlayerState(PlayerState::Locomotion);

	for (TRAVERSAL_ITER(m_mStateList, it))
		(*it).second->AddRef();

	return S_OK;
}

void CPlayerController::Awake()
{
	m_pPlayerCam = CGameManager::GetInstance().Get_PlayerCamera();
}

void CPlayerController::Start()
{
	if (m_pPlayer && m_pPlayerCam && m_pRoot)
	{
		m_ctx.Bind(m_pPlayer, m_pPlayerCam);
		m_pRoot->Enter(m_ctx);
		m_bFSMStarted = true;
	}
}

void CPlayerController::Update()
{
	Update_Key();

	if (!m_bFSMStarted)
	{
		if (m_pPlayer && m_pPlayerCam && m_pRoot)
		{
			m_ctx.Bind(m_pPlayer, m_pPlayerCam);
			m_pRoot->Enter(m_ctx);
			m_bFSMStarted = true;
		}
		else
			return;
	}

	const _int x =
		(m_mKeyHold[Right] ? 1 : 0) +
		(m_mKeyHold[Left] ? -1 : 0);

	const _int y =
		(m_mKeyHold[Forward] ? 1 : 0) +
		(m_mKeyHold[Back] ? -1 : 0);

	const _bool hasInput = (x != 0) || (y != 0);

	vector3 f = m_ctx.NormalizeXZ(m_pPlayerCam->Get_ForwardVector());
	vector3 r = m_ctx.NormalizeXZ(vector3(f.z, 0.f, -f.x));

	vector3 moveDir(0.f, 0.f, 0.f);
	_float desiredYaw = m_ctx.WrapDeg(m_pPlayerCam->Get_ForwardAngle());

	if (hasInput)
	{
		moveDir = m_ctx.NormalizeXZ(f * (_float)y + r * (_float)x);

		_float offset = atan2f((_float)x, (_float)y) * (180.f / 3.141592f);
		desiredYaw = m_ctx.WrapDeg(m_pPlayerCam->Get_ForwardAngle() + offset);

		const _bool onlyBack = (y < 0) && (x == 0) && !m_mKeyHold[Forward];
		if (onlyBack)
		{
			moveDir = m_ctx.NormalizeXZ(-f);
			desiredYaw = m_ctx.WrapDeg(m_pPlayerCam->Get_ForwardAngle() + 180.f);
		}
	}

	m_bRunning = (m_ctx.LengthXZ(moveDir) > 1e-6f);

	m_ctx.SetMovePressed(m_bRunning);
	m_ctx.SetMoveWorldDir(moveDir);
	m_ctx.SetDesiredYawDeg(desiredYaw);

	m_pRoot->Update(m_ctx);
}

void CPlayerController::LateUpdate()
{
}

void CPlayerController::OnDestroy()
{
	for (TRAVERSAL_ITER(m_mStateList, it))
		Safe_Release((*it).second);
}

CPlayerState* CPlayerController::Get_PlayerState(PlayerState _state)
{
	auto it = m_mStateList.find(_state);

	if (it == m_mStateList.end())
		return nullptr;

	return (*it).second;
}

void CPlayerController::Set_Player(CPlayer* _player)
{
	m_pPlayer = _player;

	if (m_pPlayerCam) 
		m_ctx.Bind(m_pPlayer, m_pPlayerCam);
}

void CPlayerController::Set_Camera(CPlayerCamera* _cam)
{
	m_pPlayerCam = _cam;

	if (m_pPlayer)
		m_ctx.Bind(m_pPlayer, m_pPlayerCam);
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