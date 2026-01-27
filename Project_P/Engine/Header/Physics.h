#pragma once

#include "epch.h"

NS_BEGIN(Engine)

class ENGINE_DLL CPhysics
{
public:
	struct Ray 
	{ 
		vector3 origin; 
		vector3 dir; 
		_float maxDist = 999999.f;
	};

	typedef struct RaycastHitInformation
	{
		_bool isHit = false;
		vector3 hitPos = {};
		vector3 hitNormal = {};
		_float distance = 0.f;
		CGameObject* object = nullptr;
	}RAYCASTHIT;

	SINGLETONCLASS(CPhysics);

public:
	vector<RAYCASTHIT> Raycast(const Ray& _ray);

private:
	_bool IntersectRayTriangle(
		const vector3& rayOrigin, const vector3& rayDir,
		const vector3& v0, const vector3& v1, const vector3& v2,
		_float& t, vector3& hitNormal);
};

NS_END

