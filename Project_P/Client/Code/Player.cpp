#include "cpch.h"
#include "Player.h"

CPlayer::CPlayer()
	: m_pSkinnedMeshRenderer(nullptr)
	, m_pAnimator(nullptr)
	, m_pEquipWeapon(nullptr)
	, m_sPlayerStatus({})
	, m_eAnimationStatus(Idle)
	, m_fCrtMoveSpeed(0.f)
	, m_vMoveDirection({})
	, m_vPrevMoveDirectoin({})
	, m_fRotateDirection(0.f)
	, m_fPrevRotateDirection(0.f)
	, m_bNotMoveTurning(false)
	, m_bBackMove(false)
	, m_bLockOnMode(false)
	, m_bIsCombatMode(false)
	, m_bPrevLockOnMode(false)
	, m_bIsAttack(false)
	, m_bIsPrevAttack(false)
	, m_bSwordActionDuring(false)
	, m_fSwordActionEndFrames()
	, m_fAttackComboNT(0.f)
	, m_iAttackComboDest(0)
	, m_bIsJump(false)
	, m_bIsPrevJump(false)
	, m_pFocusTransform(nullptr)
{
	m_strName = L"Player";
}

CPlayer::~CPlayer()
{
}

CPlayer* CPlayer::Create()
{
	return new CPlayer();
}

CComponent* CPlayer::Clone() const
{
	CPlayer* clone = new CPlayer();

	return clone;
}

HRESULT CPlayer::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	CTexture* tex = CResources::GetInstance().LoadOnScene<CTexture>(L"Link_Texture (Texture)");

	m_pGameObject->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(L"Link_Model (MeshBuffer)"), CResources::GetInstance().LoadSkinnedBonesOnScene(L"Link_Model (MeshBuffer)"), 0.01f, vector3::up() * 180.f);

	m_pAnimator = m_pGameObject->AddComponent<CAnimator>();
	m_pAnimator->Add_Animation(L"Idle", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_Idle (Animation)"));
	m_pAnimator->Add_Animation(L"Walk", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_Walk (Animation)"));
	m_pAnimator->Add_Animation(L"LeftWalk", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_LeftWalk (Animation)"));
	m_pAnimator->Add_Animation(L"RightWalk", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_RightWalk (Animation)"));
	m_pAnimator->Add_Animation(L"BackWalk", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_BackWalk (Animation)"));
	m_pAnimator->Add_Animation(L"CombatBackWalk", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_CombatBackWalk (Animation)"));
	m_pAnimator->Add_Animation(L"Run", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_Run (Animation)"));
	m_pAnimator->Add_Animation(L"CombatRun", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_CombatRun (Animation)"));
	m_pAnimator->Add_Animation(L"CombatIdle", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_CombatIdle (Animation)"));
	m_pAnimator->Add_Animation(L"Jump", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_Jump (Animation)"));
	m_pAnimator->Add_Animation(L"SwordAttack1", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_SwordAttack1 (Animation)"));
	m_pAnimator->Add_Animation(L"SwordCombo", CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Link_AttackCombo (Animation)"));

	m_pAnimator->SetLoop(true);

	CGameObject* swordObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Wooden Sword");
	swordObj->CreateMeshHierachy(CResources::GetInstance().LoadMeshBuffersOnScene(L"WoodenSword (MeshBuffer)"), 150.f);

	swordObj->Get_Transform()->SetParent(Get_Transform()->Find_ChildRecursive(L"RightHand"));
	
	m_fSwordActionEndFrames[0] = 1.f;
	m_fSwordActionEndFrames[1] = 2.f;
	m_fSwordActionEndFrames[2] = 3.f;

	CGameManager::GetInstance().Set_Player(this);

	//Get_Transform()->Get_Child(0)->Set_LocalScale(0.01f);
	//Get_Transform()->Get_Child(1)->Set_LocalScale(0.01f);
	//Get_Transform()->Get_Child(0)->Set_LocalEulerAnglesY(180.f);
	//Get_Transform()->Get_Child(1)->Set_LocalEulerAnglesY(180.f);

	//m_pAnimator->Set_PlaybackSpeed(0.1f);
	//m_pAnimator->Play(L"Run");

	return S_OK;
}

void CPlayer::Awake()
{
	m_sPlayerStatus.crtHp = m_sPlayerStatus.maxHp;
}

void CPlayer::Start()
{
	CGameManager::GetInstance().Get_PlayerHUD()->Update_Heart(m_sPlayerStatus.crtHp, m_sPlayerStatus.maxHp);
}

void CPlayer::Update()
{
	PlayerControle();

	if (CInput::GetInstance().GetKeyDown(P))
	{
		RecoverHp(1);
	}

	if (CInput::GetInstance().GetKeyDown(O))
	{
		GetDamage(1);
	}
}

void CPlayer::OnDestroy()
{
}

void CPlayer::Set_Focus(CTransform* _transform)
{
	m_pFocusTransform = _transform;
}

void CPlayer::RecoverHp(const _uint _value)
{
	m_sPlayerStatus.crtHp += _value;
	m_sPlayerStatus.crtHp = min(m_sPlayerStatus.crtHp, m_sPlayerStatus.maxHp);
	CGameManager::GetInstance().Get_PlayerHUD()->Update_Heart(m_sPlayerStatus.crtHp, m_sPlayerStatus.maxHp);
}

void CPlayer::GetDamage(const _uint _damage)
{
	m_sPlayerStatus.crtHp -= _damage;
	m_sPlayerStatus.crtHp = max(m_sPlayerStatus.crtHp, 0);
	CGameManager::GetInstance().Get_PlayerHUD()->Update_Heart(m_sPlayerStatus.crtHp, m_sPlayerStatus.maxHp);
}

void CPlayer::PlayerControle()
{
	m_bLockOnMode = CInput::GetInstance().GetKey(SHIFT);
	m_bIsAttack = CInput::GetInstance().GetMouseButtonDown(0);
	m_bIsJump = CInput::GetInstance().GetKeyDown(SPACE);

	if (m_bIsJump && !m_bIsPrevJump)
	{
		PlayJumpAnimation();
	}

	if (m_bIsAttack && !m_bIsPrevAttack)
	{
		PlaySwordAnimation();
		m_bSwordActionDuring = true;
		m_fAttackComboNT = 0.f;
		return;
	}

	if (m_bSwordActionDuring)
		PlayerControle_AttackCombo();

	if (m_bLockOnMode != m_bPrevLockOnMode)
	{
		if (m_vMoveDirection == vector3::zero())
			PlayIdleAnimation();
		else
			PlayMoveAnimation();
	}

	m_vMoveDirection = vector3::zero();
	m_fRotateDirection = 0.f;

	if (CInput::GetInstance().GetKey(W))
	{
		m_vMoveDirection += vector3::forward();
	}
	if (CInput::GetInstance().GetKey(S))
	{
		m_vMoveDirection += vector3::back();
	}

	if ((m_vMoveDirection.z != m_vPrevMoveDirectoin.z && m_vMoveDirection != vector3::zero()))
	{
		PlayMoveAnimation();
		m_pAnimator->Pause();
	}

	if (!m_bLockOnMode)
		PlayerControle_NoneLockOn();
	else
		PlayerControle_LockOn();

	if (m_vMoveDirection != m_vPrevMoveDirectoin && m_vMoveDirection == vector3::zero())
	{
		PlayIdleAnimation();
	}

	m_bBackMove = m_vMoveDirection.z < 0.f;

	if (m_bBackMove)
		m_fCrtMoveSpeed = m_sPlayerStatus.moveSpeed * m_sPlayerStatus.backWalkRatio;
	else
		m_fCrtMoveSpeed = m_sPlayerStatus.moveSpeed;

	if (m_bSwordActionDuring)
		m_fCrtMoveSpeed = m_sPlayerStatus.moveSpeed * 0.3f;

	if (m_vMoveDirection != m_vPrevMoveDirectoin)
	{
		PlayMoveAnimation();
	}

	if (m_bLockOnMode != m_bPrevLockOnMode)
	{
		if (!CInput::GetInstance().GetKey(SHIFT))
		{
			if (m_fRotateDirection != 0.f)
				PlayMoveAnimation();
		}
	}

	if (m_vMoveDirection != vector3::zero())
	{
		vector3 moveDir = m_vMoveDirection.normalized();
		Get_Transform()->Add_Position(Get_Transform()->Get_Directions().forward * moveDir.z * m_fCrtMoveSpeed * DELTA_TIME);
		Get_Transform()->Add_Position(Get_Transform()->Get_Directions().right * moveDir.x * m_fCrtMoveSpeed * DELTA_TIME);

		if (m_pAnimator)
			m_pAnimator->Play();
	}

	m_vPrevMoveDirectoin = m_vMoveDirection;
	m_bPrevLockOnMode = m_bLockOnMode;
	m_bIsPrevJump = m_bIsJump;
}

void CPlayer::PlayerControle_NoneLockOn()
{
	if (CInput::GetInstance().GetKey(A))
	{
		m_fRotateDirection -= 1.f;
	}
	if (CInput::GetInstance().GetKey(D))
	{
		m_fRotateDirection += 1.f;
	}

	if (m_fRotateDirection != 0.f)
	{
		if (m_fRotateDirection != 0.f) 
			Get_Transform()->Add_EulerAnglesY(m_fRotateDirection * m_sPlayerStatus.turnSpeed * DELTA_TIME);
	}

	if (m_fRotateDirection != m_fPrevRotateDirection && m_vMoveDirection == vector3::zero())
	{
		if (m_fRotateDirection != 0.f)
		{
			PlayMoveAnimation();
			m_bNotMoveTurning = true;
		}
		else
			m_bNotMoveTurning = false;
	}
	else
		m_bNotMoveTurning = false;

	if (m_fRotateDirection != m_fPrevRotateDirection && m_vMoveDirection == vector3::zero())
	{
		if (!CInput::GetInstance().GetKey(A) == !CInput::GetInstance().GetKey(D))
		{
			if (m_pAnimator)
			{
				PlayIdleAnimation();
			}
		}
	}

	m_fPrevRotateDirection = m_fRotateDirection;
}

void CPlayer::PlayerControle_LockOn()
{
	if (m_pFocusTransform)
	{
		if (vector3::Distance(m_pFocusTransform->Get_Position(), Get_Transform()->Get_Position()) > 1.5f)
		{
			quaternion targetQ = Get_Transform()->LookQuaternion(m_pFocusTransform->Get_Position(), CTransform::X | CTransform::Z);
			Get_Transform()->Set_Quaternion(targetQ.Slerp(Get_Transform()->Get_Quaternion(), targetQ, DELTA_TIME * m_sPlayerStatus.focusTurnRatio));
		}
	}

	if (CInput::GetInstance().GetKey(A))
	{
		m_vMoveDirection.x -= 1.f;
	}
	if (CInput::GetInstance().GetKey(D))
	{
		m_vMoveDirection.x += 1.f;
	}

	if (m_vMoveDirection.x != m_vPrevMoveDirectoin.x)
	{
		if (m_vMoveDirection.z <= 0.f)
		{
			PlayMoveAnimation();
		}
	}
}

void CPlayer::PlayerControle_AttackCombo()
{
	m_fAttackComboNT += DELTA_TIME;

	_float dest = 0.f;

	dest = m_fSwordActionEndFrames[m_iAttackComboDest];

	if (m_fAttackComboNT > dest)
	{
		m_fAttackComboNT = 0.f;
		m_bSwordActionDuring = false;

		if (m_vMoveDirection == vector3::zero())
			PlayIdleAnimation(0.25f);
		else
			PlayMoveAnimation(0.25f);
	}
}

void CPlayer::PlayIdleAnimation(const _float _blending)
{
	if (m_bIsJump || m_bSwordActionDuring)
		return;

	if (m_pAnimator)
	{
		if (!m_bLockOnMode)
		{
			m_pAnimator->Play(L"Idle", _blending);
		}
		else
		{
			m_pAnimator->Play(L"CombatIdle", _blending);
		}
	}

	//CDebug::Log("Idle");
}

void CPlayer::PlayMoveAnimation(const _float _blending)
{
	if (m_bIsJump || m_bSwordActionDuring)
		return;

	if (m_pAnimator)
	{
		m_pAnimator->SetLoop(true);

		if (!m_bLockOnMode)
		{
			if (m_vMoveDirection.z > 0.f)
				m_pAnimator->Play(L"Run", _blending);
			else if (m_vMoveDirection.z < 0.f)
				m_pAnimator->Play(L"BackWalk", _blending);
			else if (m_fRotateDirection != 0)
				m_pAnimator->Play(L"Walk", _blending);
		}
		else
		{
			if (m_vMoveDirection.z > 0.f)
				m_pAnimator->Play(L"CombatRun", _blending);
			else
			{
				if (m_vMoveDirection.x < 0.f)
					m_pAnimator->Play(L"LeftWalk", _blending);
				else if (m_vMoveDirection.x > 0.f)
					m_pAnimator->Play(L"RightWalk", _blending);
				else if (m_vMoveDirection.z < 0.f)
					m_pAnimator->Play(L"CombatBackWalk", _blending);
				else
					PlayIdleAnimation(_blending);
			}
		}
	}

	//CDebug::Log("CombatRun");
}

void CPlayer::PlayJumpAnimation(const _float _blending)
{
	if (m_bSwordActionDuring)
		return;

	if (m_pAnimator)
	{
		m_pAnimator->SetLoop(false);
		m_pAnimator->Play(L"Jump", _blending);
	}

	//CDebug::Log("Jump");
}

void CPlayer::PlaySwordAnimation()
{
	if (m_bIsJump || m_bSwordActionDuring)
		return;

	if (m_pAnimator)
	{
		m_pAnimator->SetLoop(true);
		m_pAnimator->Play(L"SwordCombo", 0.1f);
	}

	//CDebug::Log("SwordCombo");
}
