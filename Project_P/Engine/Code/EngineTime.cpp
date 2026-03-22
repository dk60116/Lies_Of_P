#include "epch.h"
#include "EngineTime.h"

CTime::CTime()
	: m_pTimer(nullptr)
	, m_fTimeScale(0.f)
{
}

CTime::~CTime()
{
	Release();
}

CTime& CTime::GetInstance()
{
	static CTime inst;
	return inst;
}

HRESULT CTime::Initialize()
{
	if (FAILED(Ready_Time()))
		return E_FAIL;

	return S_OK;
}

HRESULT CTime::Ready_Time()
{
	m_pTimer = new CTimer();

	if (!m_pTimer)
		return E_FAIL;

	m_pTimer->AddRef();

	if (FAILED(m_pTimer->Ready_Timer()))
	{
		delete m_pTimer;
		m_pTimer = nullptr;
		E_FAIL;
	}

	return S_OK;
}

void CTime::Update()
{
	if (!m_pTimer)
		return;

	m_pTimer->Update();
}

const _int CTime::Get_FPS() const
{
	return m_pTimer->Get_FPS();
}

const _float CTime::Get_DeltaTime() const
{
	return m_pTimer->Get_TimeDelta();
}

const _float CTime::Get_ElaspedTime() const
{
	return m_pTimer->Get_ElapsedTime();
}

void CTime::SetTimeScale(const _float _value)
{
	m_fTimeScale = _value;
}

void CTime::Release()
{
	Safe_Release(m_pTimer);
}
