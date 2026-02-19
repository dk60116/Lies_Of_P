#pragma once
#include "epch.h"

using namespace JPH;

NS_BEGIN(Engine)

class CColliderManager final
{
    SINGLETONCLASS(CColliderManager);

public:
    HRESULT Initialize();
    void FixedUpdate();
    void Release();
};

NS_END

