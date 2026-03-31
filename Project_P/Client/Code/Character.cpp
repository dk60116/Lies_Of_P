#include "cpch.h"
#include "Character.h"
#include "HurtBox.h"
#include "HitBox.h"

CCharacter::CCharacter()
	: m_strCharacterName(L"")
	, m_bIsDead(false)
	, m_pSkinnedMeshRenderer()
	, m_pBodyCollider(nullptr)
	, m_pRigidBody(nullptr)
	, m_pAnimator(nullptr)
	, m_pNavAgent(nullptr)
	, m_bIsGround(false)
	, m_iGroundMask(0)
	, m_vHurtBoxInfoList({})
	, m_vHitBoxInfoList({})
	, m_mHurtBoxList({})
{
}

CCharacter::~CCharacter()
{
}

HRESULT CCharacter::Initialize()
{
	vector<_uint> ignore = { CSceneManager::GetInstance().NameToLayer(L"Player") };
	m_iGroundMask = CSceneManager::GetInstance().MakeLayerMask(true, ignore);

	m_pNavAgent = m_pGameObject->AddComponent<CNaviMeshAgent>();

	return S_OK;
}

void CCharacter::Awake()
{
	CreateHurtBox();
	CreateHitBox();
	SetAnimationAction();
}

void CCharacter::Start()
{
}

void CCharacter::Update()
{
}

void CCharacter::FixedUpdate()
{
	Ground();
}

void CCharacter::OnDestroy()
{
	for (TRAVERSAL_ITER(m_mHurtBoxList, it))
		Safe_Release((*it).second);

	m_vHurtBoxInfoList.clear();
	m_vHitBoxInfoList.clear();
}

void CCharacter::Die()
{
	if (m_bIsDead)
		return;

	m_bIsDead = true;

	if (m_pAnimator)
		m_pAnimator->SetBool(L"isDead", true);

	m_pBodyCollider->SetEnable(false);
	m_pNavAgent->SetEnable(false);
}

const _bool CCharacter::IsDead() const
{
	return m_bIsDead;
}

CAnimator* CCharacter::GetAnimator()
{
	return m_pAnimator;
}

CRigidBody* CCharacter::GetRigidBody()
{
	return m_pRigidBody;
}

void CCharacter::AddHurtBox(const wstring& _name, CHurtBox* _box)
{
	if (!_box)
		return;

	m_mHurtBoxList.insert({ _name, _box });
	
	_box->AddRef();
}

const wstring& CCharacter::GetCharacterName() const
{
	return m_strCharacterName;
}

void CCharacter::SetAbleNavAgent(const _bool _value)
{
	m_pNavAgent->SetEnable(_value);
	m_pRigidBody->SetUseGravity(!_value);
	m_pRigidBody->ResetVelocity();
}

const _float CCharacter::GetHeight() const
{
	return m_pBodyCollider->GetHeight();
}

void CCharacter::CreateHurtBox()
{
	for (size_t i = 0; i < m_vHurtBoxInfoList.size(); ++i)
	{
		CGameObject* go = m_pGameObject->Get_Scene()->Add_GameObject(L"HurtBox_" + m_vHurtBoxInfoList[i].boneName);
		CTransform* tb = GetTransform()->Find_ChildRecursive(m_vHurtBoxInfoList[i].boneName);

		go->GetTransform()->SetParent(tb);
		go->GetTransform()->Set_LocalPosition(vector3::zero());
		go->GetTransform()->Set_LocalEulerAngles(vector3::zero());
		go->GetTransform()->Set_LocalScale(vector3::one());

		CHurtBox* hb = go->AddComponent<CHurtBox>();

		hb->CreateHurtBox(this, m_vHurtBoxInfoList[i].boneName, m_vHurtBoxInfoList[i].shape, m_vHurtBoxInfoList[i].size, m_vHurtBoxInfoList[i].center);
	}
}

void CCharacter::CreateHitBox()
{
	for (size_t i = 0; i < m_vHitBoxInfoList.size(); ++i)
	{
		CGameObject* go = m_pGameObject->Get_Scene()->Add_GameObject(L"HitBox_" + m_vHitBoxInfoList[i].boneName);
		CTransform* tb = GetTransform()->Find_ChildRecursive(m_vHitBoxInfoList[i].boneName);

		go->GetTransform()->SetParent(tb);
		go->GetTransform()->Set_LocalPosition(vector3::zero());
		go->GetTransform()->Set_LocalEulerAngles(vector3::zero());
		go->GetTransform()->Set_LocalScale(vector3::one());

		CHitBox* hb = go->AddComponent<CHitBox>();

		hb->CreateHitBox(this, m_vHitBoxInfoList[i].boneName, m_vHitBoxInfoList[i].shape, m_vHitBoxInfoList[i].size, m_vHitBoxInfoList[i].center);
	}
}

void CCharacter::Ground()
{
	CPhysics::RAYCASTHIT hit = {};
	CPhysics::SphereRay ray = {};
	ray.center = GetTransform()->Get_Position() + vector3::up() * 1.f;
	ray.radius = 0.5f;
	ray.dir = vector3::down();
	ray.maxDist = 0.75f;

	auto hits = CPhysics::GetInstance().SphereRaycast(ray, m_iGroundMask);

	m_bIsGround = hits.size() > 0;

	m_pAnimator->SetBool(L"isGround", m_bIsGround);

	if (CInput::GetInstance().GetKeyDown(KEY_CODE::V))
	{
		CPhysics::RAYCASTHIT ehit = {};
		CPhysics::Ray eray = {};
		eray.origin = GetTransform()->Get_Position() + vector3::up() * 0.5f;
		eray.dir = vector3::up();
		eray.maxDist = 1.f;

		auto hits = CPhysics::GetInstance().Raycast(eray, m_iGroundMask);
	}
}
