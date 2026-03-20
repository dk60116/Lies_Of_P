#include "cpch.h"
#include "Player.h"
#include "PlayerController.h"
#include "Eve_Sword.h"

namespace
{
	constexpr _float kGuardBlockMinDot = 0.25f;
}

CPlayer::CPlayer()
	: m_pController(nullptr)
	, m_pHeadObj(nullptr)
	, m_pHairObj(nullptr)
	, m_pPonyTailObj(nullptr)
	, m_pEquipWeapon(nullptr)
	, m_sPlayerStatus({})
	, m_sEquipStatus({})
	, m_vBodySuits({})
	, m_vFaces({})
	, m_vHairs({})
	, m_vPonyTailas({})
	, m_iLightAttackComboCount(0)
	, m_pWeaponHolder(nullptr)
{
	m_strName = L"Player";
	m_strCharacterName = L"Eve";
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

	m_pGameObject->SetLayer(L"Player");
	m_pGameObject->SetTag(L"PlayerBody");

	m_pController = m_pGameObject->AddComponent<CPlayerController>();
	m_pController->Set_Player(this);

	CTexture* suit_BaseTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Suit_Base (Texture)");
	CTexture* suit_NormalTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Suit_Normal (Texture)");
	CTexture* suit_ORMTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Suit_ORM (Texture)");

	CTexture* skin_BaseTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Skin_Base (Texture)");
	CTexture* skin_NormalTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Skin_Normal (Texture)");
	
	CTexture* boost_BaseTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Boost_Base (Texture)");
	CTexture* boost_NormalTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Boost_Normal (Texture)");
	CTexture* boost_ORMTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Boost_ORM (Texture)");

	CTexture* wing_BaseTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Wing_Base (Texture)");
	CTexture* wing_NormalTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Wing_Normal (Texture)");
	CTexture* wing_ORMTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Wing_ORM (Texture)");
	CTexture* wing_AlphaTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Wing_Alpha (Texture)");

	CTexture* frill_BaseTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Frill_Base (Texture)");
	CTexture* frill_NormalTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Frill_Normal (Texture)");
	CTexture* frill_ORMTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Frill_ORM (Texture)");

	m_vBodySuits = m_pGameObject->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(L"EveBody_Model (MeshBuffer)"), CResources::GetInstance().LoadSkinnedBonesOnScene(L"EveBody_Model (MeshBuffer)"), 0.01f, vector3::up() * 270.f);

	CTransform* addRoot = GetTransform()->Find_ChildRecursive(L"Root");

	for (TRAVERSAL_ITER(m_vBodySuits, it))
		(*it)->AddRootBone(addRoot);

	m_vBodySuits[0]->Get_Material()->Set_Texture(suit_BaseTex);
	m_vBodySuits[0]->Get_Material()->Set_Texture(suit_NormalTex, 1);
	m_vBodySuits[0]->Get_Material()->Set_Texture(suit_ORMTex, 2);

	m_vBodySuits[1]->Get_Material()->Set_Texture(boost_BaseTex);
	m_vBodySuits[1]->Get_Material()->Set_Texture(boost_NormalTex, 1);
	m_vBodySuits[1]->Get_Material()->Set_Texture(boost_ORMTex, 2);

	m_vBodySuits[2]->Get_Material()->Set_Texture(wing_BaseTex);
	m_vBodySuits[2]->Get_Material()->Set_Texture(wing_NormalTex, 1);
	m_vBodySuits[2]->Get_Material()->Set_Texture(wing_ORMTex, 2);

	m_vBodySuits[3]->Get_Material()->Set_Texture(skin_BaseTex);
	m_vBodySuits[3]->Get_Material()->Set_Texture(skin_NormalTex, 1);
	m_vBodySuits[3]->Get_Material()->Set_FloatValue(L"gRoughness", 0.9f);
	m_vBodySuits[3]->Get_Material()->Set_FloatValue(L"gMetallic", 0.f);

	m_vBodySuits[4]->Set_Material(CResources::GetInstance().CloneOnGame<CMaterial>(L"G_TransparentLit (Material)"));
	m_vBodySuits[4]->Get_Material()->Set_Texture(wing_BaseTex);
	m_vBodySuits[4]->Get_Material()->Set_Texture(wing_NormalTex, 1);
	m_vBodySuits[4]->Get_Material()->Set_Texture(wing_ORMTex, 2);
	m_vBodySuits[4]->Get_Material()->Set_Texture(wing_AlphaTex, 3);
	m_vBodySuits[4]->SetCastShadow(false);

	m_vBodySuits[5]->Set_Material(CResources::GetInstance().CloneOnGame<CMaterial>(L"G_BufferCutoutLit (Material)"));
	m_vBodySuits[5]->Get_Material()->Set_Texture(frill_BaseTex);
	m_vBodySuits[5]->Get_Material()->Set_Texture(frill_NormalTex, 1);
	//m_vBodySuits[5]->Get_Material()->Set_Texture(frill_ORMTex, 2);

	m_pHeadObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Eve_Head");
	m_vFaces = m_pHeadObj->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(L"EveHead_Model (MeshBuffer)"), CResources::GetInstance().LoadSkinnedBonesOnScene(L"EveHead_Model (MeshBuffer)"), 0.01f, vector3::up() * 270.f);

	CTexture* faceBase = CResources::GetInstance().LoadOnScene<CTexture>(L"EveFace_Base (Texture)");
	CTexture* faceNormal = CResources::GetInstance().LoadOnScene<CTexture>(L"EveFace_Normal (Texture)");
	CTexture* faceORM = CResources::GetInstance().LoadOnScene<CTexture>(L"EveFace_ORM (Texture)");

	m_vFaces[2]->Get_Material()->Set_Texture(faceBase);
	m_vFaces[2]->Get_Material()->Set_Texture(faceNormal, 1);
	//m_vFaces[2]->Get_Material()->Set_Texture(faceORM, 2);

	m_pHairObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Eve_Hear");
	m_vHairs = m_pHairObj->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(L"EveHairMain_Model (MeshBuffer)"), CResources::GetInstance().LoadSkinnedBonesOnScene(L"EveHairMain_Model (MeshBuffer)"), 0.01f, vector3::up() * 270.f);

	m_pPonyTailObj = m_pGameObject->Get_Scene()->Add_GameObject(L"EveHairPonyTail_Short");
	m_vPonyTailas = m_pPonyTailObj->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(L"EveHairPonyTail_Short_Model (MeshBuffer)"), CResources::GetInstance().LoadSkinnedBonesOnScene(L"EveHairPonyTail_Short_Model (MeshBuffer)"), 0.01f, vector3::up() * 270.f);

	for (TRAVERSAL_ITER(m_vHairs, it))
	{
		(*it)->Get_Material()->Set_BaseColor(ColorValue::black().f4Color());
		(*it)->Get_Material()->Set_FloatValue(L"gRoughness", 0.8f);
		(*it)->Get_Material()->Set_FloatValue(L"gMetallic", 0.2f);
	}

	for (TRAVERSAL_ITER(m_vPonyTailas, it))
	{
		(*it)->Get_Material()->Set_BaseColor(ColorValue::black().f4Color());
		(*it)->Get_Material()->Set_FloatValue(L"gRoughness", 0.8f);
		(*it)->Get_Material()->Set_FloatValue(L"gMetallic", 0.2f);
	}

	m_pAnimator = m_pGameObject->AddComponent<CAnimator>();
	CAnimatorController* animCon = CResources::GetInstance().LoadOnScene<CAnimatorController>(L"Eve_AnimatorController (Animator Controller)");
	m_pAnimator->Set_Controller(animCon);
	m_pAnimator->SetApplyRootmotion(true, GetTransform());

	CTransform* headSlot = GetTransform()->Find_ChildRecursive(L"Bip001-Head");
	m_pHeadObj->GetTransform()->SetParent(headSlot);
	//m_pHeadObj->GetTransform()->Set_LocalPosition(vector3(0.012f, -151.302f, -11.860f));
	m_pHeadObj->GetTransform()->Set_LocalEulerAngles(vector3(-4.5f, 180.f, 0.f));

	m_pHairObj->GetTransform()->SetParent(m_pHeadObj->GetTransform());
	m_pHairObj->GetTransform()->Set_LocalPosition(vector3::up() * 3.039f);

	m_pPonyTailObj->GetTransform()->SetParent(m_pHairObj->GetTransform());
	m_pPonyTailObj->GetTransform()->Set_LocalPosition(-0.086f, -3.039f, 0.f);
	m_pHairObj->GetTransform()->Set_LocalEulerAngles(vector3::zero());

	CGameManager::GetInstance().Set_Player(this);

	m_pWeaponHolder = GetTransform()->Find_ChildRecursive(L"SC_WeaponConstraint");

	CGameObject* m_pWeaponObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Eve_Sword");
	m_pEquipWeapon = m_pWeaponObj->AddComponent<CEve_Sword>();

	m_pWeaponObj->GetTransform()->SetParent(m_pWeaponHolder);
	m_pWeaponObj->GetTransform()->Set_LocalPosition(vector3::zero());
	m_pWeaponObj->GetTransform()->Set_LocalEulerAngles(vector3(0.f, -90.f, -90.f));
	m_pWeaponObj->GetTransform()->Set_LocalScale(1.f);

	m_pBodyCollider = m_pGameObject->AddComponent<CCapsuleCollider>();
	m_pBodyCollider->SetCenter(vector3::up() * 1.75f);
	m_pBodyCollider->SetHeight(2.5f);

	m_pRigidBody = m_pGameObject->AddComponent<CRigidBody>();
	m_pRigidBody->SetConstRotationX(true);
	m_pRigidBody->SetConstRotationY(true);
	m_pRigidBody->SetConstRotationZ(true);
	m_pRigidBody->SetUseGravity(true);

	m_sPlayerStatus.crtHp = m_sPlayerStatus.maxHp + m_sEquipStatus.hp;
	m_sPlayerStatus.crtPotion = m_sPlayerStatus.maxPotion;

	return S_OK;
}

void CPlayer::Awake()
{
	__super::Awake();

	GetTransform()->Set_PositionZ(6.f);
}

void CPlayer::Start()
{
	__super::Start();
	SyncHUDStatus();
}

void CPlayer::Update()
{
	__super::Update();

	TickDashAttackCooldown();

	if (CInput::GetInstance().GetKeyDown(Y))
	{
		GetDamage(1);
	}

	if (CInput::GetInstance().GetKeyDown(P))
	{
		RecoverHp(1);
	}

	if (CInput::GetInstance().GetKeyDown(O))
	{
		GetDamage(1);
	}
}

void CPlayer::FixedUpdate()
{
	__super::FixedUpdate();
}

void CPlayer::OnDestroy()
{
	__super::OnDestroy();

	CGameManager::GetInstance().Remove_Player(this);
}

CPlayerController* CPlayer::Get_Controller()
{
	return m_pController;
}

const CPlayer::PlayerStatus& CPlayer::Get_PlayerStatus()
{
	return m_sPlayerStatus;
}

const CPlayer::PlayerStatus CPlayer::Get_PlayerEquipStat()
{
	PlayerStatus result = m_sPlayerStatus;

	result.maxHp += m_sEquipStatus.hp;
	result.maxBetaEnergy += m_sEquipStatus.be;
	result.maxShield += m_sEquipStatus.sh;
	result.attackPower += m_sEquipStatus.attack;
	result.shAttack += m_sEquipStatus.shAttack;
	result.criticalChance += m_sEquipStatus.criticalChance;
	result.criticalDamage += m_sEquipStatus.criticalDamageRate;

	return result;
}

void CPlayer::RecoverHp(const _uint _value)
{
	m_sPlayerStatus.crtHp += _value;
	m_sPlayerStatus.crtHp = min(m_sPlayerStatus.crtHp, m_sPlayerStatus.maxHp);
	SyncHUDStatus();
}

void CPlayer::GetDamage(const _uint _damage)
{
	m_sPlayerStatus.crtHp -= _damage;
	m_sPlayerStatus.crtHp = max(m_sPlayerStatus.crtHp, 0);
	SyncHUDStatus();
}

void CPlayer::TickDashAttackCooldown()
{
	if (m_sPlayerStatus.crtDashAttackCool <= 0.f)
		return;

	m_sPlayerStatus.crtDashAttackCool = max(0.f, m_sPlayerStatus.crtDashAttackCool - DELTA_TIME);
}

void CPlayer::StartDashAttackCooldown()
{
	m_sPlayerStatus.crtDashAttackCool = m_sPlayerStatus.dashAttackCool;
}

_bool CPlayer::IsDashAttackReady() const
{
	return m_sPlayerStatus.crtDashAttackCool <= 0.f;
}

_float CPlayer::GetDashAttackCooldownRatio() const
{
	if (m_sPlayerStatus.dashAttackCool <= 0.f)
		return 0.f;

	return std::clamp(m_sPlayerStatus.crtDashAttackCool / m_sPlayerStatus.dashAttackCool, 0.f, 1.f);
}

const _float CPlayer::GetRadius() const
{
	return m_pBodyCollider->GetRadius();
}

const _uint CPlayer::GetLightAttackComboCount() const
{
	return m_iLightAttackComboCount;
}

void CPlayer::SetLightAttakComboCount(const _uint _count)
{
	m_iLightAttackComboCount = _count;
}

void CPlayer::OnSwordAttackHandler()
{
	m_pEquipWeapon->EnableHurtBox();
}

void CPlayer::DisableSwordCollider()
{
	m_pEquipWeapon->DisableHurtBox();
}

void CPlayer::GetHitHandler(const HurtDescription& _hurtDesc)
{
	if (TryGuardHit(_hurtDesc))
		return;

	if (_hurtDesc.damage > 0)
		GetDamage(static_cast<_uint>(_hurtDesc.damage));

	if (m_pController)
	{
		vector3 knockbackDir = GetTransform()->Get_Position() - _hurtDesc.position;
		knockbackDir.y = 0.f;

		if (knockbackDir.lengthSq() <= 0.0001f)
			knockbackDir = GetTransform()->Get_Directions().forward;

		knockbackDir.y = 0.f;

		if (knockbackDir.lengthSq() > 0.0001f)
			m_pController->QueueHitKnockback(knockbackDir.normalized());

		m_pController->RequestAction(CPlayerController::PlayerState::Hit);
	}
}

void CPlayer::SetAnimationAction()
{
}

_bool CPlayer::CanGuardHit(const HurtDescription& _hurtDesc)
{
	if (!m_pController || !m_pController->IsGuardActive())
		return false;

	CTransform* transform = GetTransform();
	if (!transform)
		return false;

	vector3 playerForward = transform->Get_Directions().forward;
	playerForward.y = 0.f;

	if (playerForward.lengthSq() <= 0.0001f)
		return false;

	vector3 toAttacker = _hurtDesc.position - transform->Get_Position();
	toAttacker.y = 0.f;

	if (toAttacker.lengthSq() <= 0.0001f)
		toAttacker = playerForward;

	return vector3::dot(playerForward.normalized(), toAttacker.normalized()) >= kGuardBlockMinDot;
}

_bool CPlayer::TryGuardHit(const HurtDescription& _hurtDesc)
{
	if (!CanGuardHit(_hurtDesc))
		return false;

	vector3 knockbackDir = GetTransform()->Get_Position() - _hurtDesc.position;
	knockbackDir.y = 0.f;

	if (knockbackDir.lengthSq() <= 0.0001f)
		knockbackDir = GetTransform()->Get_Directions().forward;

	m_pController->PlayGuardHit(knockbackDir);
	return true;
}

void CPlayer::SyncHUDStatus()
{
	if (CPlayerHUD* hud = CGameManager::GetInstance().Get_PlayerHUD())
		hud->Update_AllStatus(Get_PlayerEquipStat());
}

