#include "epch.h"

CColliderManager::CColliderManager()
{
}

CColliderManager::~CColliderManager()
{
	Release();
}

CColliderManager& CColliderManager::GetInstance()
{
	static CColliderManager inst;
	return inst;
}

HRESULT CColliderManager::Initialize()
{
	return S_OK;
}

void CColliderManager::FixedUpdate()
{
}

void CColliderManager::Release()
{
}