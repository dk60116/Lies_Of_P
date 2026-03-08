#include "cpch.h"
#include "Monster.h"

CMonster::CMonster()
	: m_strMonsterName(L"")
	, m_strSkinnedMeshBufferName(L"")
	, m_vMaterialTransparent({})
	, m_vMeshRenderers({})
	, m_pAnimator(nullptr)
	, m_pController(nullptr)
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

	return S_OK;
}

void CMonster::Awake()
{
}

void CMonster::Start()
{
}

void CMonster::Update()
{
}

void CMonster::OnDestroy()
{
}

const wstring& CMonster::GetMonsterName() const
{
	m_strMonsterName;
}

CAnimator* CMonster::Get_Animator()
{
	return m_pAnimator;
}

void CMonster::Change_State(const _uint _state)
{
}

const CMonster::MonsterStatus& CMonster::Get_Status()
{
	return m_sStatus;
}

void CMonster::CreateBody()
{
	const wstring path = L"Mon_" + m_strMonsterName + L"_Body_Model (MeshBuffer)";

	m_vMeshRenderers = m_pGameObject->CreateSkinnedMeshHierachy(CResources::GetInstance().LoadSkinnedMeshBuffersOnScene(path), CResources::GetInstance().LoadSkinnedBonesOnScene(path), 0.01f, vector3::up() * 270.f);
}

void CMonster::PaintTexture()
{
	for (size_t i = 0; i < m_vMeshRenderers.size(); ++i)
	{
		const wstring tdPath = L"Tex_Mon_" + m_strMonsterName + L'_' + to_wstring(i) + L"_TD (Texture)";
		const wstring tnPath = L"Tex_Mon_" + m_strMonsterName + L'_' + to_wstring(i) + L"_TN (Texture)";
		const wstring tormPath = L"Tex_Mon_" + m_strMonsterName + L'_' + to_wstring(i) + L"_TORM (Texture)";

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

	const wstring path = L"Mon_" + m_strMonsterName + L"_AnimatorController (Animator Controller)";

	CAnimatorController* animCon = CResources::GetInstance().LoadOnScene<CAnimatorController>(path);
	m_pAnimator->Set_Controller(animCon);
}
