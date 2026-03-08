#include "cpch.h"
#include "Mon_Creeper.h"

CMon_Creeper::CMon_Creeper()
{
	m_strMonsterName = L"Creeper";
}

CMon_Creeper::~CMon_Creeper()
{
}

CMon_Creeper* CMon_Creeper::Create()
{
	return new CMon_Creeper();
}

CComponent* CMon_Creeper::Clone() const
{
	CMon_Creeper* clone = new CMon_Creeper();

	return clone;
}

HRESULT CMon_Creeper::Initialize()
{
	m_vMaterialTransparent = { false, true };

	if (FAILED(__super::Initialize()))
		return E_FAIL;

	return S_OK;
}

void CMon_Creeper::Awake()
{
	__super::Awake();
}

void CMon_Creeper::Start()
{
}

void CMon_Creeper::Update()
{
}

void CMon_Creeper::FixedUpdate()
{
}

void CMon_Creeper::OnDestroy()
{
}
