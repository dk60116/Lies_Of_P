#include "cpch.h"
#include "Player.h"
#include "PlayerController.h"

CPlayer::CPlayer()
	: m_pController(nullptr)
	, m_pHeadObj(nullptr)
	, m_pHairObj(nullptr)
	, m_pSkinnedMeshRenderer(nullptr)
	, m_pAnimator(nullptr)
	, m_pEquipWeapon(nullptr)
	, m_sPlayerStatus({})
	, m_eAnimationStatus(Idle)
	, m_fSwordActionEndFrames()
	, m_fAttackComboNT(0.f)
	, m_iAttackComboDest(0)
	, m_bIsJump(false)
	, m_bIsPrevJump(false)
	, m_pFocusTransform(nullptr)
	, m_vBodySuits({})
	, m_vFaces({})
	, m_vHairs({})
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
	CAnimationClip* idle = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Idle (Animation)");
	idle->SetLoop(true);
	CAnimationClip* run_during = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Run_During (Animation)");
	run_during->SetLoop(true);
	m_pAnimator->Add_Animation(L"Idle", idle);
	m_pAnimator->Add_Animation(L"Run_During", run_during);

	CGameManager::GetInstance().Set_Player(this);

	return S_OK;
}

void CPlayer::Awake()
{
	Get_Transform()->Set_PositionY(-2.864f);
	m_pHeadObj->Get_Transform()->Set_PositionY(-2.875f);
	CTransform* headSlot = Get_Transform()->Find_ChildRecursive(L"Bip001-Head");
	m_pHeadObj->Get_Transform()->SetParent(headSlot);
	m_pHairObj->Get_Transform()->Set_PositionY(0.155f);
	m_pHairObj->Get_Transform()->SetParent(m_pHeadObj->Get_Transform());
	m_sPlayerStatus.crtHp = m_sPlayerStatus.maxHp;

	//m_vBodySuits[4]->Get_GameObject()->SetActive(false);
}

void CPlayer::Start()
{
	CGameManager::GetInstance().Get_PlayerHUD()->Update_Heart(m_sPlayerStatus.crtHp, m_sPlayerStatus.maxHp);

	m_pAnimator->Play(L"Run_During");
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
}

CPlayerController* CPlayer::Get_Controller()
{
	return m_pController;
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