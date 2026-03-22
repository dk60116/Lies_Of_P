#pragma once
#include "Component.h"

NS_BEGIN(Engine)

NS_BEGIN(EngineAI)
class CNaviMesh;
NS_END

class ENGINE_DLL CNaviMeshAgent final : public CComponent
{
	friend class CGameObject;

protected:
	CNaviMeshAgent();
	~CNaviMeshAgent();

private:
	static CNaviMeshAgent* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void OnEnable() override;
	void OnDisable() override;
	void Update() override;
	void Render_Gizmo() override;
	void OnDestroy() override;

public:
	_bool SetDestination(const vector3& _destination);
	void ResetPath();
	const _bool HasPath() const;
	const _bool HasDestination() const;
	const _bool IsOnNavigation() const;
	const vector3& GetDestination() const;
	void SetMoveSpeed(const _float _speed);
	const _float GetMoveSpeed() const;
	void SetAngularSpeed(const _float _speed);
	const _float GetAngularSpeed() const;
	void SetStoppingDistance(const _float _distance);
	const _float GetStoppingDistance() const;
	void SetWaypointTolerance(const _float _tolerance);
	const _float GetWaypointTolerance() const;
	void SetAgentRadius(const _float _radius);
	const _float GetAgentRadius() const;
	void SetAgentHeight(const _float _height);
	const _float GetAgentHeight() const;
	void SetAgentCenter(const vector3& _center);
	const vector3& GetAgentCenter() const;
	void SetGroundSnapOffset(const _float _offset);
	const _float GetGroundSnapOffset() const;
	void SetAlwaysLookAt(const _bool _alwaysLookAt);
	const _bool GetAlwaysLookAt() const;
	void SetNavigationMeshResourceName(const wstring& _resourceName);
	const wstring& GetNavigationMeshResourceName() const;
	const vector3& GetResolvedDestination() const;
	const _int GetCurrentPolygonIndex() const;
	const _int GetPathPointCount() const;

private:
	EngineAI::CNaviMesh* ResolveNavigationMesh() const;
	wstring BuildDefaultNavigationMeshResourceName() const;
	vector3 GetNavigationBaseOffset();
	_float GetWorldAgentRadius();
	_float GetWorldAgentHeight();
	_bool SnapToNavigation(EngineAI::CNaviMesh* _navMesh, const vector3& _desiredPosition, vector3& _outPosition, _int* _outPolygonIndex = nullptr) const;
	void RebuildPath(EngineAI::CNaviMesh* _navMesh, const vector3& _currentPosition, const vector3& _destinationOnNavigation);
	void ClearRuntimePath();

private:
	wstring m_strNavigationMeshResourceName;
	vector3 m_vDestination;
	vector3 m_vResolvedDestination;
	vector3 m_vAgentCenter;
	vector<vector3> m_vPathPoints;
	_float m_fMoveSpeed;
	_float m_fAngularSpeed;
	_float m_fStoppingDistance;
	_float m_fWaypointTolerance;
	_float m_fAgentRadius;
	_float m_fAgentHeight;
	_float m_fGroundSnapOffset;
	_int m_iPathIndex;
	_int m_iCurrentPolygonIndex;
	_bool m_bHasDestination;
	_bool m_bHasPath;
	_bool m_bPathDirty;
	_bool m_bOnNavigation;
	_bool m_bAlwaysLookAt;
	class CMeshBuffer* m_pLineMesh;
	class CMaterial* m_pLineMaterial;
};

NS_END

