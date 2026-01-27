#pragma once

#include "epch.h"
#include "Object.h"

NS_BEGIN(Engine)

class CTimer : public UObject
{
	friend class CTime;

private:
	explicit CTimer();
	virtual ~CTimer();

public:
	const _int Get_FPS() const;
	const _float Get_TimeDelta() const;
	const _float Get_ElapsedTime() const;

public:
	HRESULT Ready_Timer();
	void Update();

private:
	void Destroy();

private:
	LARGE_INTEGER m_iFrequency;
	LARGE_INTEGER m_iStartTime;
	LARGE_INTEGER m_iFrameTime;
	LARGE_INTEGER m_iFixTime;
	LARGE_INTEGER m_iLastTime;
	LARGE_INTEGER m_iCpuTick;

	_float m_fFPSTimer;
	_int m_iFrameCount, m_iFPS;
	_float m_fTimeDelta;

private:
	_bool m_bFirstUpdate;
};

NS_END

