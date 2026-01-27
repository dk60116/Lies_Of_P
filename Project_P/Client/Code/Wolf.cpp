#include "cpch.h"
#include "Wolf.h"

CWolf::CWolf()
	: CMonster{}
{
}

CWolf::~CWolf()
{
}

CWolf* CWolf::Create()
{
	return new CWolf();
}

CComponent* CWolf::Clone() const
{
	CWolf* clone = new CWolf();

	return clone;
}

HRESULT CWolf::Initialize()
{
	m_strSkinnedMeshBufferName = L"Wolf_Model (MeshBuffer)";

	if (FAILED(__super::Initialize()))
		return E_FAIL;

	return S_OK;
}

void CWolf::Awake()
{
	__super::Awake();
}

void CWolf::Start()
{
	__super::Start();
}

void CWolf::Update()
{
	__super::Update();
}

void CWolf::OnDestroy()
{
	__super::OnDestroy();
}
