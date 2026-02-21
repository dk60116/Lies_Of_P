#include "cpch.h"
#include "Player.h"
#include "PlayerController.h"
#include "Eve_Sword.h"

CPlayer::CPlayer()
	: m_pController(nullptr)
	, m_pHeadObj(nullptr)
	, m_pHairObj(nullptr)
	, m_pSkinnedMeshRenderer(nullptr)
	, m_pAnimator(nullptr)
	, m_pEquipWeapon(nullptr)
	, m_sPlayerStatus({})
	, m_vBodySuits({})
	, m_vFaces({})
	, m_vHairs({})
	, m_iLightAttackComboCount(0)
	, m_pWeaponHolder(nullptr)
	, m_pBodyCollider(nullptr)
	, m_pRigidBody(nullptr)
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

	CTexture* frill_BaseTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Frill_Base (Texture)");
	CTexture* frill_NormalTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Frill_Normal (Texture)");
	CTexture* frill_ORMTex = CResources::GetInstance().LoadOnScene<CTexture>(L"EveBody_Frill_ORM (Texture)");

	m_vBodySuits = m_pGameObject->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(L"EveBody_Model (MeshBuffer)"), CResources::GetInstance().LoadSkinnedBonesOnScene(L"EveBody_Model (MeshBuffer)"), 0.01f, vector3::up() * 270.f);

	CTransform* addRoot = Get_Transform()->Find_ChildRecursive(L"Root");

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

	m_vBodySuits[4]->Get_Material()->Set_Texture(wing_BaseTex);
	m_vBodySuits[4]->Get_Material()->Set_Texture(wing_NormalTex, 1);
	m_vBodySuits[4]->Get_Material()->Set_Texture(wing_ORMTex, 2);
	m_vBodySuits[4]->SetCastShadow(false);

	m_vBodySuits[5]->Get_Material()->Set_Texture(frill_BaseTex);
	m_vBodySuits[5]->Get_Material()->Set_Texture(frill_NormalTex, 1);
	m_vBodySuits[5]->Get_Material()->Set_Texture(frill_ORMTex, 2);

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

	for (TRAVERSAL_ITER(m_vHairs, it))
	{
		(*it)->Get_Material()->Set_BaseColor(ColorValue::black().f4Color());
		(*it)->Get_Material()->Set_FloatValue(L"gRoughness", 0.8f);
		(*it)->Get_Material()->Set_FloatValue(L"gMetallic", 0.2f);
	}

	m_pAnimator = m_pGameObject->AddComponent<CAnimator>();
	CAnimatorController* animCon = CResources::GetInstance().LoadOnScene<CAnimatorController>(L"Eve_AnimatorController (Animator Controller)");
	m_pAnimator->Set_Controller(animCon);
	m_pAnimator->SetApplyRootmotion(true, Get_Transform());

	CGameManager::GetInstance().Set_Player(this);

	m_pWeaponHolder = Get_Transform()->Find_ChildRecursive(L"SC_WeaponConstraint");

	CGameObject* m_pWeaponObj = m_pGameObject->Get_Scene()->Add_GameObject(L"Eve_Sword");
	m_pEquipWeapon = m_pWeaponObj->AddComponent<CEve_Sword>();

	m_pWeaponObj->Get_Transform()->SetParent(m_pWeaponHolder);
	m_pWeaponObj->Get_Transform()->Set_LocalPosition(vector3::zero());
	m_pWeaponObj->Get_Transform()->Set_LocalEulerAngles(vector3::back() * 90.f);
	m_pWeaponObj->Get_Transform()->Set_LocalScale(0.5f);

	m_pBodyCollider = m_pGameObject->AddComponent<CCapsuleCollider>();
	m_pBodyCollider->SetCenter(vector3::up() * 1.65f);
	m_pBodyCollider->SetHeight(2.5f);

	m_pRigidBody = m_pGameObject->AddComponent<CRigidBody>();
	m_pRigidBody->SetConstRotationX(true);
	m_pRigidBody->SetConstRotationY(true);
	m_pRigidBody->SetConstRotationZ(true);
	m_pRigidBody->SetUseGravity(true);

	return S_OK;
}

void CPlayer::Awake()
{
	CTransform* headSlot = Get_Transform()->Find_ChildRecursive(L"Bip001-Head");
	m_pHeadObj->Get_Transform()->SetParent(headSlot);
	m_pHeadObj->Get_Transform()->Set_LocalPosition(vector3(0.012f, -151.302f, -11.860f));
	m_pHeadObj->Get_Transform()->Set_LocalEulerAngles(vector3(-4.5f, 180.f, 0.f));
	m_pHairObj->Get_Transform()->SetParent(m_pHeadObj->Get_Transform());
	m_pHairObj->Get_Transform()->Set_LocalPosition(vector3::up() * 3.02f);
	m_pHairObj->Get_Transform()->Set_LocalEulerAngles(vector3::zero());
	m_sPlayerStatus.crtHp = m_sPlayerStatus.maxHp;

	//m_vBodySuits[4]->Get_GameObject()->SetActive(false);
}

void CPlayer::Start()
{
	CGameManager::GetInstance().Get_PlayerHUD()->Update_Heart(m_sPlayerStatus.crtHp, m_sPlayerStatus.maxHp);
}

void CPlayer::Update()
{
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

void CPlayer::OnDestroy()
{
	CGameManager::GetInstance().Remove_Player(this);
}

CPlayerController* CPlayer::Get_Controller()
{
	return m_pController;
}

CAnimator* CPlayer::Get_Animator()
{
	return m_pAnimator;
}

const CPlayer::PlayerStatus& CPlayer::Get_PlayerStatus()
{
	return m_sPlayerStatus;
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

const _uint CPlayer::GetLightAttackComboCount() const
{
	return m_iLightAttackComboCount;
}

void CPlayer::SetLightAttakComboCount(const _uint _count)
{
	m_iLightAttackComboCount = _count;
}
