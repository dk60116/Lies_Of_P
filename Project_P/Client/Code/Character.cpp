#include "cpch.h"
#include "Character.h"
#include "HurtBox.h"

CCharacter::CCharacter()
	: m_pSkinnedMeshRenderer()
	, m_pBodyCollider(nullptr)
	, m_pRigidBody(nullptr)
	, m_pAnimator(nullptr)
	, m_bIsGround(false)
	, m_iGroundMask(0)
	, m_vHurtBoxInfoList({})
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

	CreateHurtBox();

	return S_OK;
}

void CCharacter::Awake()
{
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

void CCharacter::CreateHurtBox()
{
	for (size_t i = 0; i < m_vHurtBoxInfoList.size(); ++i)
	{
		CGameObject* go = m_pGameObject->Get_Scene()->Add_GameObject(L"ColliderBox");
		CTransform* tb = Get_Transform()->Find_ChildRecursive(m_vHurtBoxInfoList[i].boneName);

		go->Get_Transform()->SetParent(tb);
		go->Get_Transform()->Set_LocalPosition(vector3::zero());
		go->Get_Transform()->Set_LocalEulerAngles(vector3::zero());
		go->Get_Transform()->Set_LocalScale(vector3::one());

		CHurtBox* hb = go->AddComponent<CHurtBox>();

		hb->CreateHurtBox(this, m_vHurtBoxInfoList[i].boneName, m_vHurtBoxInfoList[i].shape, m_vHurtBoxInfoList[i].size);
	}
}

void CCharacter::Ground()
{
	CPhysics::RAYCASTHIT hit = {};
	CPhysics::SphereRay ray = {};
	ray.center = Get_Transform()->Get_Position() + vector3::up() * 1.f;
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
		eray.origin = Get_Transform()->Get_Position() + vector3::up() * 0.5f;
		eray.dir = vector3::up();
		eray.maxDist = 1.f;

		auto hits = CPhysics::GetInstance().Raycast(eray, m_iGroundMask);
	}
}
