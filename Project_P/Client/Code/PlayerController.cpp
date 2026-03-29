#include "cpch.h"
#include "PlayerController.h"
#include "Player.h"

#include "PlayerState_Locomotion.h"
#include "PlayerState_Idle.h"
#include "PlayerState_Move.h"
#include "PlayerState_Attack.h"
#include "PlayerState_DashAttack.h"
#include "PlayerState_Guard.h"
#include "PlayerState_Evade.h"
#include "PlayerState_Jump.h"
#include "PlayerState_Hit.h"
#include "HitBox.h"
#include "Monster.h"

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
	m_mStateList.insert({ PlayerState::DashAttack, new CPlayerState_DashAttack });
	m_mStateList.insert({ PlayerState::Guard, new CPlayerState_Guard() });
	m_mStateList.insert({ PlayerState::Evade, new CPlayerState_Evade() });
	m_mStateList.insert({ PlayerState::Jump, new CPlayerState_Jump() });
	m_mStateList.insert({ PlayerState::Hit, new PlayerState_Hit() });

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

	if (m_pPlayer && m_pPlayer->GetTransform() &&
		!m_pCtx->IsActionActive(PlayerState::Attack) &&
		!m_pCtx->IsActionActive(PlayerState::Attack_S) &&
		!m_pCtx->IsActionActive(PlayerState::DashAttack))
	{
		m_vPreAnimPos = m_pPlayer->GetTransform()->Get_Position();
	}

	m_pCtx->SetSprint(m_mKeyHold[Evade]);

	HandleStrongAttackInput();

	if (m_mKeyDown[Jump] && m_pCtx->IsCanJump())
		m_pCtx->BufferAction(PlayerState::Jump);
    if (m_mKeyDown[Attack] && m_pCtx->IsCanAttack())
        m_pCtx->BufferAction(PlayerState::Attack);
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
	if (!m_pCtx || !m_pPlayer)
		return;

	if (!m_pCtx->IsActionActive(PlayerState::Attack) &&
		!m_pCtx->IsActionActive(PlayerState::Attack_S) &&
		!m_pCtx->IsActionActive(PlayerState::DashAttack))
		return;

	CTransform* tr = m_pPlayer->GetTransform();
	if (!tr)
		return;

	vector3 pos = tr->Get_Position();
	vector3 fwd = tr->Get_Directions().forward;
	fwd.y = 0.f;
	if (fwd.lengthSq() < 0.0001f)
		return;
	fwd = fwd.normalized();

	const _float playerRadius = m_pPlayer->GetRadius();

	vector<_uint> ignoreLayers = {
		CSceneManager::GetInstance().NameToLayer(L"Player"),
		CSceneManager::GetInstance().NameToLayer(L"HitBox_Player"),
		CSceneManager::GetInstance().NameToLayer(L"HitBox_Enemy"),
		CSceneManager::GetInstance().NameToLayer(L"HitBox_NPC"),
		CSceneManager::GetInstance().NameToLayer(L"HurtBox_Player"),
		CSceneManager::GetInstance().NameToLayer(L"HurtBox_Ememy"),
		CSceneManager::GetInstance().NameToLayer(L"HurtBox_NPC")
	};
	auto envMask = CSceneManager::GetInstance().MakeLayerMask(true, ignoreLayers);

	vector3 frameDelta = pos - m_vPreAnimPos;
	frameDelta.y = 0.f;

	if (frameDelta.lengthSq() > 0.0001f)
	{
		const _float forwardAmount = frameDelta.dot(fwd);

		const vector3 lateral = frameDelta - fwd * forwardAmount;
		if (lateral.lengthSq() > 0.0001f)
		{
			pos = vector3(pos.x - lateral.x, pos.y, pos.z - lateral.z);
			tr->Set_Position(pos);
		}

		if (forwardAmount > 0.001f)
		{
			CPhysics::SphereRay wallRay = {};
			wallRay.center  = m_vPreAnimPos + vector3::up() * 0.5f;
			wallRay.radius  = playerRadius * 0.5f;
			wallRay.dir     = fwd;
			wallRay.maxDist = forwardAmount + playerRadius;

			auto wallHits = CPhysics::GetInstance().SphereRaycast(wallRay, envMask);

			for (const auto& hit : wallHits)
			{
				if (hit.isHit)
				{
					pos = vector3(m_vPreAnimPos.x, pos.y, m_vPreAnimPos.z);
					tr->Set_Position(pos);
					break;
				}
			}
		}
	}

	pos = tr->Get_Position();

	CPhysics::Ray groundRay = {};
	groundRay.origin  = vector3(pos.x, pos.y + 1.f, pos.z);
	groundRay.dir     = vector3(0.f, -1.f, 0.f);
	groundRay.maxDist = 2.f;

	auto groundHits = CPhysics::GetInstance().Raycast(groundRay, envMask);

	_float closestGroundDist = 999.f;
	_float groundY = pos.y;
	for (const auto& hit : groundHits)
	{
		if (hit.isHit && hit.distance < closestGroundDist)
		{
			closestGroundDist = hit.distance;
			groundY = hit.hitPos.y;
		}
	}
	if (closestGroundDist < 999.f)
		tr->Set_Position(vector3(pos.x, groundY, pos.z));

	pos = tr->Get_Position();

	const _float checkDistance = playerRadius + 0.5f;

	CPhysics::SphereRay ray = {};
	ray.center  = pos + vector3::up() * 1.f;
	ray.radius  = playerRadius * 0.5f;
	ray.dir     = fwd;
	ray.maxDist = checkDistance;

	vector<_uint> enemyLayers = { CSceneManager::GetInstance().NameToLayer(L"HitBox_Enemy") };
	auto enemyMask = CSceneManager::GetInstance().MakeLayerMask(false, enemyLayers);

	auto hits = CPhysics::GetInstance().SphereRaycast(ray, enemyMask);
	vector3 intrusionDelta = tr->Get_Position() - m_vPreAnimPos;
	intrusionDelta.y = 0.f;
	const _float forwardIntrusion = max(0.f, intrusionDelta.dot(fwd));
	constexpr _float kEnemyContactPadding = 0.05f;

	for (const auto& hit : hits)
	{
		if (!hit.isHit || !hit.object || forwardIntrusion <= 0.f)
			continue;

		CHitBox* enemyHitBox = hit.object->GetComponent<CHitBox>();
		CMonster* monster = enemyHitBox ? dynamic_cast<CMonster*>(enemyHitBox->GetCharacter()) : nullptr;
		if (!monster)
			continue;

		CTransform* monsterTransform = monster->GetTransform();
		if (!monsterTransform)
			continue;

		vector3 toPlayer = tr->Get_Position() - monsterTransform->Get_Position();
		toPlayer.y = 0.f;

		if (toPlayer.lengthSq() <= 0.0001f)
			toPlayer = fwd;

		const _float distance = toPlayer.length();
		const _float safeDistance = playerRadius + monster->GetRadius() + kEnemyContactPadding;
		const _float overlap = safeDistance - distance;
		if (overlap <= 0.f)
			continue;

		vector3 separationDir = toPlayer.lengthSq() > 0.0001f ? toPlayer.normalized() : fwd;
		const _float correction = min(overlap, forwardIntrusion);
		if (correction <= 0.f)
			continue;

		tr->Translate(separationDir * correction);
		break;
	}

	m_vPreAnimPos = tr->Get_Position();
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

void CPlayerController::RequestAction(PlayerState _state)
{
	if (!m_pCtx)
		return;
	if (!m_pCtx->IsCanHit())
		return;

	m_pCtx->BufferAction(_state);
}

void CPlayerController::QueueHitKnockback(const vector3& _dir)
{
	if (!m_pCtx)
		return;

	m_pCtx->QueueHitKnockback(_dir);
}

const _bool CPlayerController::IsGuardActive() const
{
	return m_pCtx && m_pCtx->IsActionActive(PlayerState::Guard);
}

_bool CPlayerController::PlayGuardHit(const vector3& _dir)
{
	if (!IsGuardActive())
		return false;

	CAnimator* animator = m_pCtx->Animator();
	if (!animator)
		return false;

	if (animator->Get_CurrentState() == L"Guard_Hit")
		return false;

	animator->SetTrigger(L"Hit");
	m_pCtx->StartGuardKnockback(_dir);
	return true;
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
	m_pPlayer->GetAnimator()->SetBool(L"isBattle", m_bBattleMode);
}

void CPlayerController::HandleStrongAttackInput()
{
	if (!m_pCtx || !m_pPlayer)
		return;

	const _bool canStartAttack =
		m_pCtx->IsCanAttack() &&
		!m_pCtx->IsActionActive(PlayerState::Attack) &&
		!m_pCtx->IsActionActive(PlayerState::DashAttack);

	if (!canStartAttack)
	{
		if (!m_mKeyHold[Attack_S] || m_mKeyUp[Attack_S])
			ResetStrongAttackInput();
		return;
	}

	if (!m_pPlayer->IsDashAttackReady())
	{
		if (m_mKeyDown[Attack_S])
			QueueBufferedAttack(true);

		if (!m_mKeyHold[Attack_S] || m_mKeyUp[Attack_S])
			ResetStrongAttackInput();
		return;
	}

	if (m_mKeyDown[Attack_S])
	{
		m_fStrongAttackHoldTime = 0.f;
		m_bPendingStrongAttackRelease = true;
		m_bDashAttackQueuedFromHold = false;
	}

	if (m_mKeyHold[Attack_S] && m_bPendingStrongAttackRelease && !m_bDashAttackQueuedFromHold)
	{
		m_fStrongAttackHoldTime += DELTA_TIME;

		if (m_fStrongAttackHoldTime >= 0.3f)
		{
			m_pCtx->BufferAction(PlayerState::DashAttack);
			m_bDashAttackQueuedFromHold = true;
			m_bPendingStrongAttackRelease = false;
		}
	}

	if (m_mKeyUp[Attack_S])
	{
		if (m_bPendingStrongAttackRelease && !m_bDashAttackQueuedFromHold)
			QueueBufferedAttack(true);

		ResetStrongAttackInput();
	}
}

void CPlayerController::QueueBufferedAttack(_bool _strong)
{
	if (!m_pCtx || !m_pCtx->IsCanAttack())
		return;

	m_pCtx->BufferAction(_strong ? PlayerState::Attack_S : PlayerState::Attack);
}

void CPlayerController::ResetStrongAttackInput()
{
	m_fStrongAttackHoldTime = 0.f;
	m_bPendingStrongAttackRelease = false;
	m_bDashAttackQueuedFromHold = false;
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

