#include "epch.h"
#include "Timer.h"

CTimer::CTimer()
	: m_iFrequency({})
	, m_iStartTime({})
    , m_iFrameTime({})
    , m_iFixTime({})
    , m_iLastTime({})
    , m_iCpuTick({})
    , m_fFPSTimer(0.f)
    , m_iFrameCount(0)
    , m_iFPS(0)
    , m_fTimeDelta(0.f)
    , m_bFirstUpdate(false)
{
}

CTimer::~CTimer()
{
}

HRESULT CTimer::Ready_Timer()
{
	QueryPerformanceFrequency(&m_iFrequency);
	QueryPerformanceCounter(&m_iStartTime);
	QueryPerformanceCounter(&m_iFrameTime);			// 1077
	QueryPerformanceCounter(&m_iLastTime);			// 1085
	QueryPerformanceCounter(&m_iFixTime);			// 1090

	QueryPerformanceFrequency(&m_iCpuTick);

	return S_OK;
}

void CTimer::Update()
{
	QueryPerformanceCounter(&m_iFrameTime);

	if (m_iFrameTime.QuadPart - m_iFixTime.QuadPart >= m_iCpuTick.QuadPart)
	{
		QueryPerformanceFrequency(&m_iCpuTick);
		m_iFixTime = m_iFrameTime;
	}

	m_fTimeDelta = (m_iFrameTime.QuadPart - m_iLastTime.QuadPart) / static_cast<_float>(m_iCpuTick.QuadPart);

	if (m_bFirstUpdate)
	{
		m_fTimeDelta = 0.f;
		m_bFirstUpdate = false;
	}
	else
	{
		const float MAX_DELTA = 0.1f;
		m_fTimeDelta = min(m_fTimeDelta, MAX_DELTA);
	}

	m_iLastTime = m_iFrameTime;

	// FPS °è»ê
	m_fFPSTimer += m_fTimeDelta;
	++m_iFrameCount;

	if (m_fFPSTimer >= 1.f)
	{
		m_iFPS = m_iFrameCount;
		m_iFrameCount = 0;
		m_fFPSTimer = 0.f;
	}
}

const int CTimer::Get_FPS() const
{
    return m_iFPS;
}

const _float CTimer::Get_TimeDelta() const
{
	return m_fTimeDelta;
}

const _float CTimer::Get_ElapsedTime() const
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	LONGLONG elapsedCounts = now.QuadPart - m_iStartTime.QuadPart;
	return static_cast<_float>(elapsedCounts) / static_cast<_float>(m_iFrequency.QuadPart);
}
