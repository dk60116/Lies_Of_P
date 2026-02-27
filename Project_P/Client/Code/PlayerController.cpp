#include "cpch.h"
#include "PlayerController.h"
#include "Player.h"

#include "PlayerState_Locomotion.h"
#include "PlayerState_Idle.h"
#include "PlayerState_Move.h"
#include "PlayerState_Attack.h"
#include "PlayerState_StrongAttack.h"
#include "PlayerState_Guard.h"
#include "PlayerState_Evade.h"
#include "PlayerState_Jump.h"

CPlayerController::CPlayerController()
	: m_pCtx(nullptr)
	, m_pPlayer(nullptr)
	, m_pPlayerCam(nullptr)
	, m_bFSMStarted(false)
	, m_mStateList({})
	, m_pRoot(nullptr)
	, m_bRunning(false)
	, m_bBattleMode(false)
	, m_fPrevSpeed(0.f)
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
	m_pCtx = new CPlayerControllerContext();

	m_mStateList.insert({ PlayerState::Locomotion, new CPlayerState_Locomotion() });
	m_mStateList.insert({ PlayerState::Idle, new CPlayerState_Idle() });
	m_mStateList.insert({ PlayerState::Move, new CPlayerState_Move() });
	m_mStateList.insert({ PlayerState::Attack, new CPlayerState_Attack() });
	m_mStateList.insert({ PlayerState::Attack_S, new CPlayerState_StrongAttack() });
	m_mStateList.insert({ PlayerState::Guard, new CPlayerState_Guard() });
	m_mStateList.insert({ PlayerState::Evade, new CPlayerState_Evade() });
	m_mStateList.insert({ PlayerState::Jump, new CPlayerState_Jump() });

	auto loco = static_cast<CPlayerState_Locomotion*>(Get_PlayerState(PlayerState::Locomotion));

	loco->SetChildren(m_mStateList);

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
		m_pCtx->Bind(m_pPlayer, m_pPlayerCam, this);

	for (TRAVERSAL_ITER(m_mStateList, it))
		(*it).second->Initialize(m_pCtx, (*it).first);

	if (m_pPlayer && m_pPlayerCam && m_pRoot)
	{
		m_pRoot->Enter();
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
            m_pCtx->Bind(m_pPlayer, m_pPlayerCam, this);
            m_pRoot->Enter();
            m_bFSMStarted = true;
        }
        else
            return;
    }

	m_pCtx->SetSprint(m_mKeyHold[Evade]);

	if (m_mKeyDown[Jump] && m_pCtx->IsCanJump())
		m_pCtx->BufferAction(PlayerState::Jump);
    if (m_mKeyDown[Attack] && m_pCtx->IsCanAttack())
        m_pCtx->BufferAction(PlayerState::Attack);
	if (m_mKeyDown[Attack_S] && m_pCtx->IsCanAttack())
		m_pCtx->BufferAction(PlayerState::Attack_S);
	if (m_mKeyHold[Guard] && m_pCtx->IsCanGuard() && !m_pCtx->IsActionActive(PlayerState::Guard))
		m_pCtx->BufferAction(PlayerState::Guard);
	if (m_mKeyDown[Evade] && m_pCtx->IsCanEvade())
		m_pCtx->BufferAction(PlayerState::Evade);

	_float speed = 0.f;

	if (m_pCtx->Animator()->GetFloat(L"speed", speed))
		m_pCtx->Animator()->SetBool(L"sprintEnd", m_fPrevSpeed == 2);

    const _int x =
        (m_mKeyHold[Right] ? 1 : 0) +
        (m_mKeyHold[Left] ? -1 : 0);

    const _int y =
        (m_mKeyHold[Forward] ? 1 : 0) +
        (m_mKeyHold[Back] ? -1 : 0);

	m_pCtx->Animator()->SetFloat(L"dirX", static_cast<_float>(x));
	m_pCtx->Animator()->SetFloat(L"dirZ", static_cast<_float>(y));

    const _bool hasInput = (x != 0) || (y != 0);

	if (!m_pCtx->IsEvadeExit())
	{
		if (m_pCtx->CurrentState() != PlayerState::Evade)
			m_pCtx->Animator()->SetBool(L"isInputDir", x + y != 0);
	}

    const _bool attackLock = m_pCtx->IsActionActive(PlayerState::Attack);

	if (hasInput)
	{
		vector3 f = m_pCtx->NormalizeXZ(m_pPlayerCam->Get_ForwardVector());
		vector3 r = m_pCtx->NormalizeXZ(vector3(f.z, 0.f, -f.x));

		vector3 moveDir = m_pCtx->NormalizeXZ(f * (_float)y + r * (_float)x);

		_float camYaw = m_pCtx->WrapDeg(m_pPlayerCam->Get_ForwardAngle());
		_float offsetDeg = atan2f((_float)x, (_float)y) * (180.f / 3.141592f);
		_float desiredYaw = m_pCtx->WrapDeg(camYaw + offsetDeg);

		const _bool onlyBack = (y < 0) && (x == 0) && !m_mKeyHold[Forward];

		if (onlyBack)
		{
			moveDir = m_pCtx->NormalizeXZ(-f);
			desiredYaw = m_pCtx->WrapDeg(camYaw + 180.f);
		}
		
		m_pCtx->SetMoveWorldDir(moveDir);
		m_pCtx->SetDesiredYawDeg(desiredYaw);
		m_pCtx->BeginTurnTo(desiredYaw);
	}

	m_pCtx->SetMoveLocalDir(vector3((_float)x, 0.f, (_float)y));

	m_bRunning = hasInput;

    m_pRoot->Update();

	m_fPrevSpeed = speed;
}

void CPlayerController::LateUpdate()
{
}

void CPlayerController::OnDestroy()
{
	delete m_pCtx;
	m_pCtx = nullptr;

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
		m_pCtx->Bind(m_pPlayer, m_pPlayerCam, this);
}

void CPlayerController::Set_Camera(CPlayerCamera* _cam)
{
	m_pPlayerCam = _cam;

	if (m_pPlayer)
		m_pCtx->Bind(m_pPlayer, m_pPlayerCam, this);
}

const _bool CPlayerController::IsRunning() const
{
	return m_bRunning;
}

const _bool CPlayerController::IsBattle() const
{
	return m_bBattleMode;
}

void CPlayerController::SetBattle(const _bool _value)
{
	m_bBattleMode = _value;
	m_pPlayer->Get_Animator()->SetBool(L"isBattle", m_bBattleMode);
}

void CPlayerController::Update_Key()
{
	KEY_CODE key_F = KEY_CODE::W;
	KEY_CODE key_B = KEY_CODE::S;
	KEY_CODE key_L = KEY_CODE::A;
	KEY_CODE key_R = KEY_CODE::D;
	KEY_CODE key_Guard = KEY_CODE::E;
	KEY_CODE key_Evade = KEY_CODE::L_SHIFT;
	KEY_CODE key_Jump = KEY_CODE::SPACE;

	_uint mouse0 = 0;
	_uint mouse1 = 1;

	m_mKeyHold[Forward] = CInput::GetInstance().GetKey(key_F);
	m_mKeyHold[Back] = CInput::GetInstance().GetKey(key_B);
	m_mKeyHold[Left] = CInput::GetInstance().GetKey(key_L);
	m_mKeyHold[Right] = CInput::GetInstance().GetKey(key_R);

	m_mKeyDown[Forward] = CInput::GetInstance().GetKeyDown(key_F);
	m_mKeyDown[Back] = CInput::GetInstance().GetKeyDown(key_B);
	m_mKeyDown[Left] = CInput::GetInstance().GetKeyDown(key_L);
	m_mKeyDown[Right] = CInput::GetInstance().GetKeyDown(key_R);

	m_mKeyUp[Forward] = CInput::GetInstance().GetKeyUp(key_F);
	m_mKeyUp[Back] = CInput::GetInstance().GetKeyUp(key_B);
	m_mKeyUp[Left] = CInput::GetInstance().GetKeyUp(key_L);
	m_mKeyUp[Right] = CInput::GetInstance().GetKeyUp(key_R);

	m_mKeyHold[Attack] = CInput::GetInstance().GetMouseButton(mouse0);
	m_mKeyDown[Attack] = CInput::GetInstance().GetMouseButtonDown(mouse0);
	m_mKeyUp[Attack] = CInput::GetInstance().GetMouseButtonUp(mouse0);

	m_mKeyHold[Attack_S] = CInput::GetInstance().GetMouseButton(mouse1);
	m_mKeyDown[Attack_S] = CInput::GetInstance().GetMouseButtonDown(mouse1);
	m_mKeyUp[Attack_S] = CInput::GetInstance().GetMouseButtonUp(mouse1);

	m_mKeyHold[Guard] = CInput::GetInstance().GetKey(key_Guard);
	m_mKeyDown[Guard] = CInput::GetInstance().GetKeyDown(key_Guard);
	m_mKeyUp[Guard] = CInput::GetInstance().GetKeyUp(key_Guard);

	m_mKeyHold[Evade] = CInput::GetInstance().GetKey(key_Evade);
	m_mKeyDown[Evade] = CInput::GetInstance().GetKeyDown(key_Evade);
	m_mKeyUp[Evade] = CInput::GetInstance().GetKeyUp(key_Evade);

	m_mKeyHold[Jump] = CInput::GetInstance().GetKey(key_Jump);
	m_mKeyDown[Jump] = CInput::GetInstance().GetKeyDown(key_Jump);
	m_mKeyUp[Jump] = CInput::GetInstance().GetKeyUp(key_Jump);
}
