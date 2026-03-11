#include "cpch.h"
#include "BT_Creeper.h"

CBT_Creeper::CBT_Creeper()
{
}

CBT_Creeper::~CBT_Creeper()
{
}

CBT_Creeper* CBT_Creeper::Create()
{
	return new CBT_Creeper();
}

CComponent* CBT_Creeper::Clone() const
{
	CBT_Creeper* clone = new CBT_Creeper();

	return clone;
}

HRESULT CBT_Creeper::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	SetRoot(CreateHideChaseRoot());

	return S_OK;
}

void CBT_Creeper::Awake()
{
	__super::Awake();
}

void CBT_Creeper::OnEnable()
{
	__super::OnEnable();
}

void CBT_Creeper::OnDisable()
{
	__super::OnDisable();
}

void CBT_Creeper::Update()
{
	__super::Update();
}

void CBT_Creeper::OnDestroy()
{
	__super::OnDestroy();
}
