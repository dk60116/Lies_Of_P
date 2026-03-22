#pragma once

#include "epch.h"
#include "Timer.h"

NS_BEGIN(Engine)

class ENGINE_DLL CTime final
{
	SINGLETONCLASS(CTime);

public:
	HRESULT Initialize();

public:
	HRESULT Ready_Time();
	void Update();
	void Release();

public:
	const _int Get_FPS() const;
	const _float Get_DeltaTime() const;
	const _float Get_ElaspedTime() const;
	void SetTimeScale(const _float _value);

private:
	CTimer* m_pTimer;
	float m_fTimeScale;
};

NS_END

