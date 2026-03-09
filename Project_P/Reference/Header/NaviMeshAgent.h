#pragma once
#include "Component.h"

NS_BEGIN(Engine)

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
	void OnDestroy() override;

public:
	_bool SetDestination(const vector3& destination);
	void ResetPath();

	const vector3& GetDestination() const;
	const vector3& GetVelocity() const;
	_float GetRemainingDistance() const;
	const _bool HasPath() const;
	const _bool IsStopped() const;
	void SetStopped(const _bool stopped);

	_float GetRadius() const;
	void SetRadius(const _float value);
	_float GetHeight() const;
	void SetHeight(const _float value);
	_float GetSpeed() const;
	void SetSpeed(const _float value);
	_float GetAcceleration() const;
	void SetAcceleration(const _float value);
	_float GetAngularSpeed() const;
	void SetAngularSpeed(const _float value);
	_float GetStoppingDistance() const;
	void SetStoppingDistance(const _float value);
	const _bool GetAutoBraking() const;
	void SetAutoBraking(const _bool value);
	const _bool GetUpdateRotation() const;
	void SetUpdateRotation(const _bool value);

private:
	void AdvancePathIfNeeded(const vector3& position);
	void UpdateRemainingDistance(const vector3& position);
	void StopMotion();

private:
	_float m_fRadius;
	_float m_fHeight;
	_float m_fSpeed;
	_float m_fAcceleration;
	_float m_fAngularSpeed;
	_float m_fStoppingDistance;
	_bool m_bAutoBraking;
	_bool m_bUpdateRotation;
	_bool m_bStopped;
	_bool m_bHasPath;
	vector3 m_vDestination;
	vector3 m_vVelocity;
	vector<vector3> m_vPathCorners;
	size_t m_iPathCornerIndex;
	_float m_fRemainingDistance;
};

NS_END