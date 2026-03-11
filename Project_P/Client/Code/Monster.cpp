#include "cpch.h"
#include "Monster.h"
#include "MonsterController.h"

CMonster::CMonster()
	: m_iCurrentState(0)
	, m_vMaterialTransparent({})
	, m_vMeshRenderers({})
	, m_pController(nullptr)
	, m_pNavAgent(nullptr)
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

	return S_OK;
}

void CMonster::Awake()
{
	__super::Awake();
}

void CMonster::Start()
{
	__super::Start();
}

void CMonster::Update()
{
	__super::Update();
}

void CMonster::OnDestroy()
{
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
	m_pNavAgent = m_pGameObject->AddComponent<CNaviMeshAgent>();
	m_pController = m_pGameObject->AddComponent<CMonsterController>();

	m_pNavAgent->SetMoveSpeed(m_sStatus.moveSpeed);
	m_pNavAgent->SetStoppingDistance(m_sStatus.attackRange);
	m_pNavAgent->SetAngularSpeed(m_sStatus.turnSpeed);
}

CNaviMeshAgent* CMonster::GetNaviAgent()
{
	return m_pNavAgent;
}

void CMonster::GetHitHandler(const HurtDescription& _hurtDesc)
{
}
