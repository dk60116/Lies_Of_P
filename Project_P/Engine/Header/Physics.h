#pragma once

#include "epch.h"
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

NS_BEGIN(Engine)

namespace Layers
{
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer SENSOR = 2;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 3;
};

namespace BroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH::BroadPhaseLayer MOVING(1);
	static constexpr _uint NUM_BP_LAYERS = 2;
};

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

