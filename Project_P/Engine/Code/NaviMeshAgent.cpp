#include "epch.h"
#include "NaviMeshAgent.h"
#include "NaviMesh.h"
#include "Scene.h"
#include "SceneManager.h"
#include "Transform.h"
#include "EngineTime.h"

namespace
{
	_bool IsAgentOnNavigationMesh(EngineAI::CNaviMesh* navMesh, const vector3& position, const _float height, const _float radius)
	{
		if (!navMesh)
			return false;
		if (navMesh->FindContainingPolygon(position) < 0)
			return false;

		vector3 sampledPosition = {};
		if (!navMesh->SamplePosition(position, sampledPosition))
			return false;

		const _float tolerance = max(0.05f, radius * 0.25f);
		const _float directDelta = fabsf(sampledPosition.y - position.y);
		const _float bottomDelta = fabsf(sampledPosition.y - (position.y - height * 0.5f));
		return min(directDelta, bottomDelta) <= tolerance;
	}

	_bool IsPointOnNavigationMeshSurface(EngineAI::CNaviMesh* navMesh, const vector3& point, const _float tolerance)
	{
		if (!navMesh)
			return false;
		if (navMesh->FindContainingPolygon(point) < 0)
			return false;

		vector3 sampledPosition = {};
		if (!navMesh->SamplePosition(point, sampledPosition))
			return false;

		return fabsf(sampledPosition.y - point.y) <= tolerance;
	}
}

CNaviMeshAgent::CNaviMeshAgent()
	: m_fRadius(0.5f)
	, m_fHeight(2.f)
	, m_fSpeed(3.5f)
	, m_fAcceleration(8.f)
	, m_fAngularSpeed(360.f)
	, m_fStoppingDistance(0.1f)
	, m_bAutoBraking(true)
	, m_bUpdateRotation(true)
	, m_bStopped(false)
	, m_bHasPath(false)
	, m_vDestination(vector3::zero())
	, m_vVelocity(vector3::zero())
	, m_vPathCorners({})
	, m_iPathCornerIndex(0)
	, m_fRemainingDistance(0.f)
{
	m_strName = L"NaviMeshAgent";
}

CNaviMeshAgent::~CNaviMeshAgent()
{
}

CNaviMeshAgent* CNaviMeshAgent::Create()
{
	return new CNaviMeshAgent();
}

CComponent* CNaviMeshAgent::Clone() const
{
	CNaviMeshAgent* clone = new CNaviMeshAgent();
	clone->m_fRadius = m_fRadius;
	clone->m_fHeight = m_fHeight;
	clone->m_fSpeed = m_fSpeed;
	clone->m_fAcceleration = m_fAcceleration;
	clone->m_fAngularSpeed = m_fAngularSpeed;
	clone->m_fStoppingDistance = m_fStoppingDistance;
	clone->m_bAutoBraking = m_bAutoBraking;
	clone->m_bUpdateRotation = m_bUpdateRotation;
	return clone;
}

HRESULT CNaviMeshAgent::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	return S_OK;
}

void CNaviMeshAgent::Awake()
{
	StopMotion();
}

void CNaviMeshAgent::OnEnable()
{
	StopMotion();
}

void CNaviMeshAgent::OnDisable()
{
	StopMotion();
}

void CNaviMeshAgent::Update()
{
	if (!CSceneManager::GetInstance().IsPlaying())
		return;
	if (m_bStopped || !m_bHasPath)
		return;

	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	if (!scene)
		return;

	EngineAI::CNaviMesh* navMesh = scene->GetNavigationMesh();
	if (!navMesh || navMesh->IsEmpty())
	{
		ResetPath();
		return;
	}

	CTransform* transform = Get_Transform();
	if (!transform)
		return;

	const _float deltaTime = max(CTime::GetInstance().Get_DeltaTime(), 0.0001f);
	vector3 position = transform->Get_Position();
	if (!IsAgentOnNavigationMesh(navMesh, position, m_fHeight, m_fRadius))
	{
		ResetPath();
		return;
	}

	AdvancePathIfNeeded(position);
	if (!m_bHasPath || m_iPathCornerIndex >= m_vPathCorners.size())
	{
		StopMotion();
		return;
	}

	const vector3 target = m_vPathCorners[m_iPathCornerIndex];
	vector3 toTarget = target - position;
	const _float distanceToTarget = toTarget.length();
	if (distanceToTarget <= max(0.01f, m_fStoppingDistance))
	{
		AdvancePathIfNeeded(position);
		if (!m_bHasPath || m_iPathCornerIndex >= m_vPathCorners.size())
		{
			StopMotion();
			return;
		}
		toTarget = m_vPathCorners[m_iPathCornerIndex] - position;
	}

	vector3 desiredVelocity = vector3::zero();
	if (toTarget.lengthSq() > 1e-6f)
	{
		desiredVelocity = toTarget.normalized() * m_fSpeed;
		if (m_bAutoBraking && m_iPathCornerIndex + 1 >= m_vPathCorners.size())
		{
			const _float brakingDistance = max(m_fStoppingDistance * 2.f, 0.1f);
			const _float speedScale = Engine::clamp(toTarget.length() / brakingDistance, 0.f, 1.f);
			desiredVelocity *= speedScale;
		}
	}

	vector3 velocityDelta = desiredVelocity - m_vVelocity;
	const _float maxVelocityDelta = m_fAcceleration * deltaTime;
	if (velocityDelta.lengthSq() > maxVelocityDelta * maxVelocityDelta)
		velocityDelta = velocityDelta.normalized() * maxVelocityDelta;
	m_vVelocity += velocityDelta;

	vector3 step = m_vVelocity * deltaTime;
	if (step.lengthSq() > toTarget.lengthSq())
		step = toTarget;

	vector3 newPosition = position + step;
	vector3 sampledPosition = newPosition;
	if (navMesh->SamplePosition(newPosition, sampledPosition))
		newPosition = sampledPosition;

	transform->Set_Position(newPosition);
	UpdateRemainingDistance(newPosition);

	if (m_bUpdateRotation)
	{
		vector3 flatVelocity = m_vVelocity;
		flatVelocity.y = 0.f;
		if (flatVelocity.lengthSq() > 1e-6f)
		{
			const quaternion targetRotation = transform->LookQuaternion(newPosition + flatVelocity.normalized(), CTransform::X | CTransform::Z);
			const _float rotationT = Engine::clamp((m_fAngularSpeed * deltaTime) / 180.f, 0.f, 1.f);
			transform->Set_Quaternion(quaternion::Slerp(transform->Get_Quaternion(), targetRotation, rotationT));
		}
	}

	AdvancePathIfNeeded(newPosition);
	if (!m_bHasPath)
		StopMotion();
}

void CNaviMeshAgent::OnDestroy()
{
	m_vPathCorners.clear();
}

_bool CNaviMeshAgent::SetDestination(const vector3& destination)
{
	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	if (!scene)
		return false;

	EngineAI::CNaviMesh* navMesh = scene->GetNavigationMesh();
	if (!navMesh || navMesh->IsEmpty())
		return false;

	CTransform* transform = Get_Transform();
	if (!transform)
		return false;

	const vector3 position = transform->Get_Position();
	if (!IsAgentOnNavigationMesh(navMesh, position, m_fHeight, m_fRadius))
	{
		ResetPath();
		return false;
	}
	if (!IsPointOnNavigationMeshSurface(navMesh, destination, max(0.05f, m_fRadius * 0.25f)))
	{
		ResetPath();
		return false;
	}

	vector<vector3> pathCorners = {};
	if (!navMesh->FindPath(transform->Get_Position(), destination, pathCorners))
	{
		ResetPath();
		return false;
	}

	m_vDestination = destination;
	m_vPathCorners = move(pathCorners);
	m_iPathCornerIndex = 0;
	m_bHasPath = !m_vPathCorners.empty();
	m_bStopped = false;
	UpdateRemainingDistance(transform->Get_Position());
	return m_bHasPath;
}

void CNaviMeshAgent::ResetPath()
{
	m_bHasPath = false;
	m_iPathCornerIndex = 0;
	m_vPathCorners.clear();
	m_fRemainingDistance = 0.f;
	m_vVelocity = vector3::zero();
}

const vector3& CNaviMeshAgent::GetDestination() const
{
	return m_vDestination;
}

const vector3& CNaviMeshAgent::GetVelocity() const
{
	return m_vVelocity;
}

_float CNaviMeshAgent::GetRemainingDistance() const
{
	return m_fRemainingDistance;
}

const _bool CNaviMeshAgent::HasPath() const
{
	return m_bHasPath;
}

const _bool CNaviMeshAgent::IsStopped() const
{
	return m_bStopped;
}

void CNaviMeshAgent::SetStopped(const _bool stopped)
{
	m_bStopped = stopped;
	if (m_bStopped)
		m_vVelocity = vector3::zero();
}

_float CNaviMeshAgent::GetRadius() const
{
	return m_fRadius;
}

void CNaviMeshAgent::SetRadius(const _float value)
{
	m_fRadius = max(value, 0.01f);
}

_float CNaviMeshAgent::GetHeight() const
{
	return m_fHeight;
}

void CNaviMeshAgent::SetHeight(const _float value)
{
	m_fHeight = max(value, 0.01f);
}

_float CNaviMeshAgent::GetSpeed() const
{
	return m_fSpeed;
}

void CNaviMeshAgent::SetSpeed(const _float value)
{
	m_fSpeed = max(value, 0.f);
}

_float CNaviMeshAgent::GetAcceleration() const
{
	return m_fAcceleration;
}

void CNaviMeshAgent::SetAcceleration(const _float value)
{
	m_fAcceleration = max(value, 0.f);
}

_float CNaviMeshAgent::GetAngularSpeed() const
{
	return m_fAngularSpeed;
}

void CNaviMeshAgent::SetAngularSpeed(const _float value)
{
	m_fAngularSpeed = max(value, 0.f);
}

_float CNaviMeshAgent::GetStoppingDistance() const
{
	return m_fStoppingDistance;
}

void CNaviMeshAgent::SetStoppingDistance(const _float value)
{
	m_fStoppingDistance = max(value, 0.f);
}

const _bool CNaviMeshAgent::GetAutoBraking() const
{
	return m_bAutoBraking;
}

void CNaviMeshAgent::SetAutoBraking(const _bool value)
{
	m_bAutoBraking = value;
}

const _bool CNaviMeshAgent::GetUpdateRotation() const
{
	return m_bUpdateRotation;
}

void CNaviMeshAgent::SetUpdateRotation(const _bool value)
{
	m_bUpdateRotation = value;
}

void CNaviMeshAgent::AdvancePathIfNeeded(const vector3& position)
{
	if (!m_bHasPath)
		return;

	while (m_iPathCornerIndex < m_vPathCorners.size())
	{
		const _float threshold = (m_iPathCornerIndex + 1 >= m_vPathCorners.size()) ? max(0.01f, m_fStoppingDistance) : 0.05f;
		if ((m_vPathCorners[m_iPathCornerIndex] - position).length() > threshold)
			break;
		++m_iPathCornerIndex;
	}

	if (m_iPathCornerIndex >= m_vPathCorners.size())
		ResetPath();
}

void CNaviMeshAgent::UpdateRemainingDistance(const vector3& position)
{
	if (!m_bHasPath || m_iPathCornerIndex >= m_vPathCorners.size())
	{
		m_fRemainingDistance = 0.f;
		return;
	}

	_float remainingDistance = (m_vPathCorners[m_iPathCornerIndex] - position).length();
	for (size_t index = m_iPathCornerIndex; index + 1 < m_vPathCorners.size(); ++index)
		remainingDistance += (m_vPathCorners[index + 1] - m_vPathCorners[index]).length();
	m_fRemainingDistance = remainingDistance;
}

void CNaviMeshAgent::StopMotion()
{
	m_vVelocity = vector3::zero();
	if (!m_bHasPath)
		m_fRemainingDistance = 0.f;
}