#include "cpch.h"
#include "Monster.h"

CMonster::CMonster()
	: m_strSkinnedMeshBufferName(L"")
	, m_fSkinnedMeshScaleFactor(0.01f)
	, m_vMeshRenderers({})
	, m_pBaseMap(nullptr)
	, m_pAnimator(nullptr)
	, m_pController(nullptr)
{
	m_strName = L"Wolf";
}

CMonster::~CMonster()
{
}

HRESULT CMonster::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

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
