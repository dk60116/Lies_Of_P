#include "cpch.h"
#include "Monster.h"
#include "MonsterController.h"
#include "HurtBox.h"

CMonster::CMonster()
	: m_iCurrentState(0)
	, m_vMaterialTransparent({})
	, m_vMeshRenderers({})
	, m_pController(nullptr)
	, m_bHitRequested(false)
	, m_bHitReacting(false)
	, m_vPendingHitKnockback(vector3::zero())
	, m_vActiveHitKnockback(vector3::zero())
	, m_qHitReactionRotation(quaternion::identity())
	, m_fHitKnockbackRemain(0.f)
	, m_pHUD(nullptr)
{
	m_strName = L"Monster";
}

CMonster::~CMonster()
{
}

HRESULT CMonster::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	CreateBody();
	PaintTexture();
	CreateAnimator();
	CreateAI();
	m_sStatus.crtHp = m_sStatus.maxHp;

	return S_OK;
}

void CMonster::Awake()
{
	__super::Awake();

	CGameObject* hudObj = m_pGameObject->Get_Scene()->Add_GameObject(L"HUD - " + m_strCharacterName);
	m_pHUD = hudObj->AddComponent<CMonsterHUD>();
	m_pHUD->BIndMonster(this);
}

void CMonster::Start()
{
	__super::Start();

	for (const auto& hurtBoxPair : m_mHurtBoxList)
	{
		if (hurtBoxPair.second)
			hurtBoxPair.second->SetDamage(m_sStatus.attackPower);
	}
}

void CMonster::Update()
{
	__super::Update();
}

void CMonster::OnDestroy()
{
	m_bHitRequested = false;
	m_bHitReacting = false;
	m_vPendingHitKnockback = vector3::zero();
	m_vActiveHitKnockback = vector3::zero();
	m_qHitReactionRotation = quaternion::identity();
	m_fHitKnockbackRemain = 0.f;
	__super::OnDestroy();
}

void CMonster::Change_State(const _uint _state)
{
	m_iCurrentState = _state;
}

const CMonster::MonsterStatus& CMonster::GetStatus()
{
	return m_sStatus;
}

const _float CMonster::GetRadius() const
{
	return m_pBodyCollider->GetRadius();
}

void CMonster::CreateBody()
{
	const wstring path = L"Mon_" + m_strCharacterName + L"_Body_Model (MeshBuffer)";

	m_vMeshRenderers = m_pGameObject->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(path), CResources::GetInstance().LoadSkinnedBonesOnScene(path), 0.01f, vector3::up() * 270.f);
	m_pBodyCollider = m_pGameObject->AddComponent<CCapsuleCollider>();
	m_pRigidBody = m_pGameObject->AddComponent<CRigidBody>();

	m_pRigidBody->SetKinematic(true);
	m_pRigidBody->SetUseGravity(false);
	m_pRigidBody->SetConstRotationX(true);
	m_pRigidBody->SetConstRotationY(true);
	m_pRigidBody->SetConstRotationZ(true);
}

void CMonster::PaintTexture()
{
	for (size_t i = 0; i < m_vMeshRenderers.size(); ++i)
	{
		const wstring tdPath = L"Tex_Mon_" + m_strCharacterName + L'_' + to_wstring(i) + L"_TD (Texture)";
		const wstring tnPath = L"Tex_Mon_" + m_strCharacterName + L'_' + to_wstring(i) + L"_TN (Texture)";
		const wstring tormPath = L"Tex_Mon_" + m_strCharacterName + L'_' + to_wstring(i) + L"_TORM (Texture)";

		CTexture* td = CResources::GetInstance().LoadOnScene<CTexture>(tdPath);
		CTexture* tn = CResources::GetInstance().LoadOnScene<CTexture>(tnPath);
		CTexture* torm = CResources::GetInstance().LoadOnScene<CTexture>(tormPath);

		if (m_vMaterialTransparent[i])
			m_vMeshRenderers[i]->Set_Material(CResources::GetInstance().LoadOnGame<CMaterial>(L"G_BufferCutoutLit (Material)"));

		m_vMeshRenderers[i]->Get_Material()->Set_Texture(td);
		m_vMeshRenderers[i]->Get_Material()->Set_Texture(tn, 1);
		m_vMeshRenderers[i]->Get_Material()->Set_Texture(torm, 2);
	}
}

void CMonster::CreateAnimator()
{
	m_pAnimator = m_pGameObject->AddComponent<CAnimator>();

	const wstring path = L"Mon_" + m_strCharacterName + L"_AnimatorController (Animator Controller)";
	CAnimatorController* animCon = CResources::GetInstance().LoadOnScene<CAnimatorController>(path);
	m_pAnimator->Set_Controller(animCon);
}

void CMonster::CreateAI()
{
	m_pController = m_pGameObject->AddComponent<CMonsterController>();

	m_pNavAgent->SetMoveSpeed(m_sStatus.moveSpeed);
	m_pNavAgent->SetStoppingDistance(m_sStatus.attackRange);
	m_pNavAgent->SetAngularSpeed(m_sStatus.turnSpeed);
}

CNaviMeshAgent* CMonster::GetNaviAgent()
{
	return m_pNavAgent;
}

const _bool CMonster::HasPendingHitReaction() const
{
	return m_bHitRequested;
}

const _bool CMonster::IsHitReacting() const
{
	return m_bHitReacting;
}

void CMonster::BeginHitReaction()
{
	m_bHitRequested = false;
	m_bHitReacting = true;
	m_vActiveHitKnockback = m_vPendingHitKnockback;
	m_vActiveHitKnockback.y = 0.f;
	m_vPendingHitKnockback = vector3::zero();
	m_fHitKnockbackRemain = m_vActiveHitKnockback.lengthSq() > 0.0001f ? m_sStatus.hitKnockbackDuration : 0.f;

	if (CTransform* transform = GetTransform())
		m_qHitReactionRotation = transform->Get_LocalQuaternion();

	if (m_fHitKnockbackRemain > 0.f)
	{
		const vector3 immediateDelta = m_vActiveHitKnockback.normalized() * max(0.15f, m_sStatus.hitKnockbackSpeed * 0.04f);
		if (CTransform* transform = GetTransform())
		{
			transform->Set_LocalQuaternion(m_qHitReactionRotation);
			transform->Translate(immediateDelta);
			transform->Set_LocalQuaternion(m_qHitReactionRotation);
		}
	}
}

void CMonster::EndHitReaction()
{
	m_bHitReacting = false;
	m_vPendingHitKnockback = vector3::zero();
	m_vActiveHitKnockback = vector3::zero();
	m_qHitReactionRotation = quaternion::identity();
	m_fHitKnockbackRemain = 0.f;
}

void CMonster::TickHitReactionKnockback()
{
	if (!m_bHitReacting)
		return;

	if (m_fHitKnockbackRemain <= 0.f || m_vActiveHitKnockback.lengthSq() <= 0.0001f)
	{
		m_fHitKnockbackRemain = 0.f;
		m_vActiveHitKnockback = vector3::zero();
		return;
	}

	const _float knockbackSpeed = max(0.f, m_sStatus.hitKnockbackSpeed);
	if (knockbackSpeed <= 0.f)
	{
		m_fHitKnockbackRemain = 0.f;
		m_vActiveHitKnockback = vector3::zero();
		return;
	}

	const vector3 delta = m_vActiveHitKnockback.normalized() * knockbackSpeed * DELTA_TIME;
	if (CTransform* transform = GetTransform())
	{
		transform->Set_LocalQuaternion(m_qHitReactionRotation);
		transform->Translate(delta);
		transform->Set_LocalQuaternion(m_qHitReactionRotation);
	}

	m_fHitKnockbackRemain = max(0.f, m_fHitKnockbackRemain - DELTA_TIME);
	if (m_fHitKnockbackRemain <= 0.f)
		m_vActiveHitKnockback = vector3::zero();
}

void CMonster::GetHitHandler(const HurtDescription& _hurtDesc)
{
	if (_hurtDesc.damage > 0)
	{
		m_sStatus.crtHp -= _hurtDesc.damage;
		m_sStatus.crtHp = max(m_sStatus.crtHp, 0);
	}

	if (CTransform* transform = GetTransform())
	{
		vector3 knockbackDir = _hurtDesc.forward;
		knockbackDir.y = 0.f;

		if (knockbackDir.lengthSq() <= 0.0001f)
			knockbackDir = transform->Get_Position() - _hurtDesc.position;

		knockbackDir.y = 0.f;

		if (knockbackDir.lengthSq() <= 0.0001f)
			knockbackDir = transform->Get_Directions().forward;

		knockbackDir.y = 0.f;
		if (knockbackDir.lengthSq() > 0.0001f)
			m_vPendingHitKnockback = knockbackDir.normalized();
	}

	m_bHitRequested = true;
}
