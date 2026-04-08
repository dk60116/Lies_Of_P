#include "epch.h"
#include "NaviMeshAgent.h"

#include "Camera.h"
#include "CapsuleCollider.h"
#include "Editor.h"
#include "GameObject.h"
#include "Material.h"
#include "MeshBuffer.h"
#include "NaviMesh.h"
#include "Resources.h"
#include "Scene.h"
#include "SceneManager.h"
#include "SphereCollider.h"
#include "Transform.h"
#include <unordered_set>

namespace
{
	constexpr _float kAgentPositionEpsilon = 0.001f;
	constexpr _float kDestinationRebuildThreshold = 0.05f;
	constexpr _float kAgentGizmoHeightOffset = 0.03f;
	constexpr _uint kAgentGizmoSegmentCount = 32u;
	constexpr _float kAgentSeparationPadding = 0.02f;
	constexpr _float kAgentSeparationDirectionEpsilon = 0.0001f;
	constexpr _float kAgentSeparationMinShare = 0.25f;
	constexpr _float kAgentSeparationMaxShare = 0.75f;
	constexpr _float kAgentSeparationMovingShare = 0.6f;
	constexpr _int kNavigationConstraintSortIndex = 1000;

	unordered_set<CNaviMeshAgent*> g_vActiveNavigationAgents = {};

	void RegisterNavigationAgent(CNaviMeshAgent* _agent)
	{
		if (_agent)
			g_vActiveNavigationAgents.insert(_agent);
	}

	void UnregisterNavigationAgent(CNaviMeshAgent* _agent)
	{
		if (_agent)
			g_vActiveNavigationAgents.erase(_agent);
	}

	_int GetCollisionWeightPriority(const CNaviMeshAgent::CollisionWeight _weight)
	{
		switch (_weight)
		{
		case CNaviMeshAgent::CollisionWeight::High:
			return 2;

		case CNaviMeshAgent::CollisionWeight::Default:
			return 1;

		case CNaviMeshAgent::CollisionWeight::Low:
		default:
			return 0;
		}
	}

	_bool BuildLineWorldMatrix(const _vector& a, const _vector& b, _matrix& outWorld)
	{
		_vector delta = b - a;
		_float length = XMVectorGetX(XMVector3Length(delta));
		if (length <= 0.0001f)
			return false;

		_vector dir = XMVector3Normalize(delta);
		_vector xAxis = XMVectorSet(1.f, 0.f, 0.f, 0.f);
		_float dot = XMVectorGetX(XMVector3Dot(xAxis, dir));
		_matrix rot = XMMatrixIdentity();

		if (dot < 0.9999f)
		{
			if (dot > -0.9999f)
			{
				_vector axis = XMVector3Normalize(XMVector3Cross(xAxis, dir));
				_float angle = acosf(dot);
				rot = XMMatrixRotationAxis(axis, angle);
			}
			else
			{
				rot = XMMatrixRotationAxis(XMVectorSet(0.f, 1.f, 0.f, 0.f), XM_PI);
			}
		}

		_vector mid = (a + b) * 0.5f;
		_matrix scale = XMMatrixScaling(length, 1.f, 1.f);
		_matrix trans = XMMatrixTranslationFromVector(mid);
		outWorld = scale * rot * trans;
		return true;
	}
}

CNaviMeshAgent::CNaviMeshAgent()
	: m_strNavigationMeshResourceName(L"")
	, m_vDestination(vector3::zero())
	, m_vResolvedDestination(vector3::zero())
	, m_vAgentCenter(vector3(0.f, 1.f, 0.f))
	, m_vPathPoints({})
	, m_fMoveSpeed(3.5f)
	, m_fAngularSpeed(720.f)
	, m_fStoppingDistance(0.15f)
	, m_fWaypointTolerance(0.1f)
	, m_fAgentRadius(0.35f)
	, m_fAgentHeight(2.0f)
	, m_fGroundSnapOffset(0.02f)
	, m_iPathIndex(0)
	, m_iCurrentPolygonIndex(-1)
	, m_bHasDestination(false)
	, m_bHasPath(false)
	, m_bPathDirty(true)
	, m_bOnNavigation(false)
	, m_bHasLastNavigationPosition(false)
	, m_bAlwaysLookAt(false)
	, m_vLastNavigationPosition(vector3::zero())
	, m_pLineMesh(nullptr)
	, m_pLineMaterial(nullptr)
	, m_eCollisionWeight(CollisionWeight::Default)
{
	m_strName = L"NaviMeshAgent";
	m_iSortIndex = kNavigationConstraintSortIndex;
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
	clone->m_strNavigationMeshResourceName = m_strNavigationMeshResourceName;
	clone->m_vDestination = m_vDestination;
	clone->m_vResolvedDestination = m_vResolvedDestination;
	clone->m_vAgentCenter = m_vAgentCenter;
	clone->m_fMoveSpeed = m_fMoveSpeed;
	clone->m_fAngularSpeed = m_fAngularSpeed;
	clone->m_fStoppingDistance = m_fStoppingDistance;
	clone->m_fWaypointTolerance = m_fWaypointTolerance;
	clone->m_fAgentRadius = m_fAgentRadius;
	clone->m_fAgentHeight = m_fAgentHeight;
	clone->m_fGroundSnapOffset = m_fGroundSnapOffset;
	clone->m_bHasDestination = m_bHasDestination;
	clone->m_bAlwaysLookAt = m_bAlwaysLookAt;
	clone->m_eCollisionWeight = m_eCollisionWeight;
	clone->m_iSortIndex = m_iSortIndex;
	clone->m_bPathDirty = true;
	return clone;
}

HRESULT CNaviMeshAgent::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

#ifndef _CLIENT_BUILD
	if (!m_pLineMesh)
	{
		m_pLineMesh = CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Line (Mesh Buffer)");
		if (m_pLineMesh)
			m_pLineMesh->AddRef();
	}

	if (!m_pLineMaterial)
	{
		m_pLineMaterial = CResources::GetInstance().CloneOnGame<CMaterial>(L"DefaultLineMaterial (Material)");
		if (m_pLineMaterial)
		{
			m_pLineMaterial->Set_BaseColor(_float4(0.1f, 0.8f, 1.f, 1.f));
			m_pLineMaterial->AddRef();
		}
	}
#endif

	ClearRuntimePath();
	m_bOnNavigation = false;
	m_iCurrentPolygonIndex = -1;
	m_bHasLastNavigationPosition = false;
	return S_OK;
}

void CNaviMeshAgent::Awake()
{
	RegisterNavigationAgent(this);
	m_bPathDirty = m_bHasDestination;
	m_bHasLastNavigationPosition = false;
}

void CNaviMeshAgent::OnEnable()
{
	RegisterNavigationAgent(this);
	m_bPathDirty = m_bHasDestination;
	m_bHasLastNavigationPosition = false;
}

void CNaviMeshAgent::OnDisable()
{
	UnregisterNavigationAgent(this);
	ClearRuntimePath();
	m_bOnNavigation = false;
	m_iCurrentPolygonIndex = -1;
	m_bHasLastNavigationPosition = false;
}

void CNaviMeshAgent::Update()
{
	CTransform* transform = GetTransform();
	if (!transform)
		return;

	EngineAI::CNaviMesh* navMesh = ResolveNavigationMesh();
	if (!navMesh || !navMesh->IsBuilt())
	{
		m_bOnNavigation = false;
		m_iCurrentPolygonIndex = -1;
		ClearRuntimePath();
		m_bPathDirty = m_bHasDestination;
		return;
	}

	const vector3 worldGroundingOffset = GetNavigationBaseOffset() - vector3(0.f, m_fGroundSnapOffset, 0.f);
	if (!ConstrainTransformToNavigation(navMesh, worldGroundingOffset, true, true))
		return;

	ApplySeparationOnNavigation(navMesh, worldGroundingOffset);

	vector3 currentPosition = transform->Get_Position();
	vector3 currentNavigationPosition = m_vLastNavigationPosition;

	auto rotateTowardsHorizontal = [&](const vector3& worldDirection, const vector3& worldOrigin)
	{
		vector3 flatDirection(worldDirection.x, 0.f, worldDirection.z);
		if (flatDirection.lengthSq() <= (kAgentPositionEpsilon * kAgentPositionEpsilon))
			return;

		const _float angularSpeed = max(0.f, m_fAngularSpeed);
		if (angularSpeed <= 0.f)
			return;

		flatDirection = flatDirection.normalized();
		const quaternion currentRotation = transform->Get_LocalQuaternion();
		const quaternion targetRotation = transform->LookQuaternion(worldOrigin + flatDirection, CTransform::X | CTransform::Z);
		const _float rotationDot = min(1.f, fabsf(currentRotation.dot(targetRotation)));
		const _float angleRadians = 2.f * acosf(rotationDot);
		if (angleRadians <= XMConvertToRadians(0.1f))
		{
			transform->Set_LocalQuaternion(targetRotation);
			return;
		}

		const _float maxStepRadians = XMConvertToRadians(angularSpeed) * DELTA_TIME;
		if (maxStepRadians <= 0.f)
			return;

		const _float rotationT = min(1.f, maxStepRadians / angleRadians);
		transform->Set_LocalQuaternion(quaternion::Slerp(currentRotation, targetRotation, rotationT));
	};

	if (!m_bHasDestination)
		return;

	vector3 destinationOnNavigation = m_vDestination;
	if (!SnapToNavigation(navMesh, m_vDestination, destinationOnNavigation))
	{
		ClearRuntimePath();
		m_bPathDirty = true;
		return;
	}

	const _float stoppingDistance = max(0.f, m_fStoppingDistance);
	if (vector3::Distance(currentNavigationPosition, destinationOnNavigation) <= stoppingDistance)
	{
		m_vResolvedDestination = destinationOnNavigation;
		if (m_bAlwaysLookAt)
		{
			const vector3 destinationWorldPosition = destinationOnNavigation + worldGroundingOffset;
			rotateTowardsHorizontal(destinationWorldPosition - currentPosition, currentPosition);
		}
		ClearRuntimePath();
		return;
	}

	if (m_bPathDirty || !m_bHasPath || m_vPathPoints.empty() ||
		vector3::Distance(m_vResolvedDestination, destinationOnNavigation) > kDestinationRebuildThreshold)
	{
		RebuildPath(navMesh, currentNavigationPosition, destinationOnNavigation);
	}

	if (!m_bHasPath || m_vPathPoints.empty())
		return;

	while (m_iPathIndex < static_cast<_int>(m_vPathPoints.size()))
	{
		const _bool isLastPoint = (m_iPathIndex == static_cast<_int>(m_vPathPoints.size()) - 1);
		const _float reachThreshold = isLastPoint
			? max(stoppingDistance, m_fWaypointTolerance)
			: m_fWaypointTolerance;

		if (vector3::Distance(currentNavigationPosition, m_vPathPoints[m_iPathIndex]) > reachThreshold)
			break;

		++m_iPathIndex;
	}

	if (m_iPathIndex >= static_cast<_int>(m_vPathPoints.size()))
	{
		ClearRuntimePath();
		return;
	}

	const vector3 targetPoint = m_vPathPoints[m_iPathIndex];
	const vector3 toTarget = targetPoint - currentNavigationPosition;
	const _float distanceToTarget = toTarget.length();
	if (distanceToTarget <= kAgentPositionEpsilon)
		return;

	const _float moveSpeed = max(0.f, m_fMoveSpeed);
	const _float stepDistance = moveSpeed * DELTA_TIME;
	if (stepDistance <= 0.f)
		return;

	const _bool isFinalPathPoint = (m_iPathIndex == static_cast<_int>(m_vPathPoints.size()) - 1);
	_float moveDistance = min(stepDistance, distanceToTarget);
	if (isFinalPathPoint)
	{
		const _float distanceToDestination = vector3::Distance(currentNavigationPosition, destinationOnNavigation);
		moveDistance = min(moveDistance, max(0.f, distanceToDestination - stoppingDistance));
	}

	if (moveDistance <= kAgentPositionEpsilon)
	{
		m_vResolvedDestination = destinationOnNavigation;
		if (m_bAlwaysLookAt)
			rotateTowardsHorizontal((destinationOnNavigation + worldGroundingOffset) - currentPosition, currentPosition);
		ClearRuntimePath();
		return;
	}

	const vector3 direction = toTarget / distanceToTarget;
	const vector3 desiredNavigationPosition = currentNavigationPosition + direction * moveDistance;
	vector3 constrainedNavigationPosition = desiredNavigationPosition;
	_int nextPolygonIndex = -1;
	if (!navMesh->ConstrainMovement(currentNavigationPosition, desiredNavigationPosition, constrainedNavigationPosition, &nextPolygonIndex))
	{
		m_bOnNavigation = false;
		m_iCurrentPolygonIndex = -1;
		m_bHasLastNavigationPosition = false;
		ClearRuntimePath();
		m_bPathDirty = true;
		return;
	}

	const vector3 newWorldPosition = constrainedNavigationPosition + worldGroundingOffset;
	transform->Set_Position(newWorldPosition);
	rotateTowardsHorizontal((targetPoint + worldGroundingOffset) - newWorldPosition, newWorldPosition);
	m_bOnNavigation = true;
	m_iCurrentPolygonIndex = nextPolygonIndex;
	m_vLastNavigationPosition = constrainedNavigationPosition;
	m_bHasLastNavigationPosition = true;

	if (vector3::Distance(constrainedNavigationPosition, destinationOnNavigation) <= stoppingDistance)
	{
		m_vResolvedDestination = destinationOnNavigation;
		ClearRuntimePath();
	}
}

void CNaviMeshAgent::LateUpdate()
{
	CTransform* transform = GetTransform();
	if (!transform)
		return;

	EngineAI::CNaviMesh* navMesh = ResolveNavigationMesh();
	if (!navMesh || !navMesh->IsBuilt())
	{
		m_bOnNavigation = false;
		m_iCurrentPolygonIndex = -1;
		m_bHasLastNavigationPosition = false;
		return;
	}

	const vector3 worldGroundingOffset = GetNavigationBaseOffset() - vector3(0.f, m_fGroundSnapOffset, 0.f);
	if (!ConstrainTransformToNavigation(navMesh, worldGroundingOffset, true, true))
		return;

	ApplySeparationOnNavigation(navMesh, worldGroundingOffset);
}

void CNaviMeshAgent::Render_Gizmo()
{
#ifndef _CLIENT_BUILD
	if (!CEditor::GetInstance().IsSelected(m_pGameObject))
		return;

	if (!m_pLineMesh || !m_pLineMaterial)
		return;

	CCamera* camera = CSceneManager::GetInstance().Get_EditorCamera();
	CTransform* transform = GetTransform();
	if (!camera || !transform || !camera->GetTransform())
		return;

	const vector3 cameraPos3 = camera->GetTransform()->Get_Position();
	const _float3 cameraPosition(cameraPos3.x, cameraPos3.y, cameraPos3.z);
	const _matrix view = camera->GetViewMatrix();
	const _matrix proj = camera->GetProjectionMatrix();

	auto drawLine = [&](const vector3& start, const vector3& end, const _float4& color)
		{
			_matrix lineWorld = XMMatrixIdentity();
			if (!BuildLineWorldMatrix(XMVectorSet(start.x, start.y, start.z, 1.f), XMVectorSet(end.x, end.y, end.z, 1.f), lineWorld))
				return;

			m_pLineMaterial->Set_BaseColor(color);
			m_pLineMaterial->Bind_Matrix(lineWorld);
			m_pLineMaterial->Bind_Camera(cameraPosition, view, proj, 0);
			m_pLineMesh->Render();
		};

	auto drawCircle = [&](const vector3& center, const _float radius, const _float4& color)
		{
			if (radius <= 0.f)
				return;

			for (_uint i = 0; i < kAgentGizmoSegmentCount; ++i)
			{
				const _float t0 = (XM_2PI * static_cast<_float>(i)) / static_cast<_float>(kAgentGizmoSegmentCount);
				const _float t1 = (XM_2PI * static_cast<_float>(i + 1)) / static_cast<_float>(kAgentGizmoSegmentCount);
				const vector3 p0(center.x + cosf(t0) * radius, center.y, center.z + sinf(t0) * radius);
				const vector3 p1(center.x + cosf(t1) * radius, center.y, center.z + sinf(t1) * radius);
				drawLine(p0, p1, color);
			}
		};

	const vector3 worldGroundingOffset = GetNavigationBaseOffset() - vector3(0.f, m_fGroundSnapOffset, 0.f);
	const vector3 objectPosition = transform->Get_Position();
	vector3 agentBasePosition = objectPosition - worldGroundingOffset;
	agentBasePosition.y += kAgentGizmoHeightOffset;

	const _float agentRadius = max(GetWorldAgentRadius(), 0.1f);
	const _float agentHeight = max(GetWorldAgentHeight(), 0.1f);
	const vector3 agentTopPosition = agentBasePosition + vector3(0.f, agentHeight, 0.f);
	const _float stoppingDistance = max(m_fStoppingDistance, 0.05f);
	const _float4 agentColor = m_bOnNavigation ? _float4(0.15f, 0.95f, 0.35f, 1.f) : _float4(1.f, 0.25f, 0.25f, 1.f);
	const _float4 pathColor = _float4(0.15f, 0.85f, 1.f, 1.f);
	const _float4 destinationColor = _float4(1.f, 0.82f, 0.2f, 1.f);

	drawCircle(agentBasePosition, agentRadius, agentColor);
	drawCircle(agentTopPosition, agentRadius, agentColor);
	drawLine(agentBasePosition, agentTopPosition, agentColor);
	drawLine(agentBasePosition + vector3(agentRadius, 0.f, 0.f), agentTopPosition + vector3(agentRadius, 0.f, 0.f), agentColor);
	drawLine(agentBasePosition + vector3(-agentRadius, 0.f, 0.f), agentTopPosition + vector3(-agentRadius, 0.f, 0.f), agentColor);
	drawLine(agentBasePosition + vector3(0.f, 0.f, agentRadius), agentTopPosition + vector3(0.f, 0.f, agentRadius), agentColor);
	drawLine(agentBasePosition + vector3(0.f, 0.f, -agentRadius), agentTopPosition + vector3(0.f, 0.f, -agentRadius), agentColor);
	drawLine(agentBasePosition + vector3(-agentRadius * 0.35f, 0.f, 0.f), agentBasePosition + vector3(agentRadius * 0.35f, 0.f, 0.f), agentColor);
	drawLine(agentBasePosition + vector3(0.f, 0.f, -agentRadius * 0.35f), agentBasePosition + vector3(0.f, 0.f, agentRadius * 0.35f), agentColor);

	if (m_bHasPath && !m_vPathPoints.empty())
	{
		vector3 previousPoint = agentBasePosition;
		const _int startIndex = max(0, min(m_iPathIndex, static_cast<_int>(m_vPathPoints.size()) - 1));
		for (_int pointIndex = startIndex; pointIndex < static_cast<_int>(m_vPathPoints.size()); ++pointIndex)
		{
			vector3 pathPoint = m_vPathPoints[pointIndex];
			pathPoint.y += kAgentGizmoHeightOffset;
			drawLine(previousPoint, pathPoint, pathColor);
			drawCircle(pathPoint, 0.04f, pathColor);
			previousPoint = pathPoint;
		}
	}
	else if (m_bHasDestination)
	{
		vector3 destinationPoint = m_vDestination;
		destinationPoint.y += kAgentGizmoHeightOffset;
		drawLine(agentBasePosition, destinationPoint, pathColor);
	}

	if (m_bHasDestination)
	{
		vector3 destinationPoint = m_bHasPath ? m_vResolvedDestination : m_vDestination;
		destinationPoint.y += kAgentGizmoHeightOffset;
		drawCircle(destinationPoint, stoppingDistance, destinationColor);
		drawLine(destinationPoint + vector3(-stoppingDistance * 0.35f, 0.f, 0.f), destinationPoint + vector3(stoppingDistance * 0.35f, 0.f, 0.f), destinationColor);
		drawLine(destinationPoint + vector3(0.f, 0.f, -stoppingDistance * 0.35f), destinationPoint + vector3(0.f, 0.f, stoppingDistance * 0.35f), destinationColor);
	}
#endif
}

void CNaviMeshAgent::OnDestroy()
{
	UnregisterNavigationAgent(this);
	Safe_Release(m_pLineMesh);
	Safe_Release(m_pLineMaterial);
	ClearRuntimePath();
}

_bool CNaviMeshAgent::SetDestination(const vector3& _destination)
{
	m_vDestination = _destination;
	m_bHasDestination = true;
	m_bPathDirty = true;
	return true;
}

void CNaviMeshAgent::ResetPath()
{
	m_bHasDestination = false;
	m_bPathDirty = false;
	m_vDestination = vector3::zero();
	m_vResolvedDestination = vector3::zero();
	ClearRuntimePath();
}

const _bool CNaviMeshAgent::HasPath() const
{
	return m_bHasPath;
}

const _bool CNaviMeshAgent::HasDestination() const
{
	return m_bHasDestination;
}

const _bool CNaviMeshAgent::IsOnNavigation() const
{
	return m_bOnNavigation;
}

const vector3& CNaviMeshAgent::GetDestination() const
{
	return m_vDestination;
}

void CNaviMeshAgent::SetMoveSpeed(const _float _speed)
{
	m_fMoveSpeed = max(0.f, _speed);
}

const _float CNaviMeshAgent::GetMoveSpeed() const
{
	return m_fMoveSpeed;
}

void CNaviMeshAgent::SetAngularSpeed(const _float _speed)
{
	m_fAngularSpeed = max(0.f, _speed);
}

const _float CNaviMeshAgent::GetAngularSpeed() const
{
	return m_fAngularSpeed;
}

void CNaviMeshAgent::SetStoppingDistance(const _float _distance)
{
	m_fStoppingDistance = max(0.f, _distance);
	m_bPathDirty = true;
}

const _float CNaviMeshAgent::GetStoppingDistance() const
{
	return m_fStoppingDistance;
}

void CNaviMeshAgent::SetWaypointTolerance(const _float _tolerance)
{
	m_fWaypointTolerance = max(0.001f, _tolerance);
	m_bPathDirty = true;
}

const _float CNaviMeshAgent::GetWaypointTolerance() const
{
	return m_fWaypointTolerance;
}

void CNaviMeshAgent::SetAgentRadius(const _float _radius)
{
	m_fAgentRadius = max(0.01f, _radius);
}

const _float CNaviMeshAgent::GetAgentRadius() const
{
	return m_fAgentRadius;
}

void CNaviMeshAgent::SetAgentHeight(const _float _height)
{
    const _float oldHeight = m_fAgentHeight;
    const _float newHeight = max(0.01f, _height);
    const _bool keepBottomAligned =
        fabsf(m_vAgentCenter.x) <= 0.0001f &&
        fabsf(m_vAgentCenter.z) <= 0.0001f &&
        fabsf(m_vAgentCenter.y - oldHeight * 0.5f) <= 0.0001f;

    m_fAgentHeight = newHeight;
    if (keepBottomAligned)
        m_vAgentCenter.y = newHeight * 0.5f;
}

const _float CNaviMeshAgent::GetAgentHeight() const
{
	return m_fAgentHeight;
}

void CNaviMeshAgent::SetAgentCenter(const vector3& _center)
{
    m_vAgentCenter = _center;
}

const vector3& CNaviMeshAgent::GetAgentCenter() const
{
    return m_vAgentCenter;
}

void CNaviMeshAgent::SetGroundSnapOffset(const _float _offset)
{
	m_fGroundSnapOffset = max(0.f, _offset);
}

const _float CNaviMeshAgent::GetGroundSnapOffset() const
{
	return m_fGroundSnapOffset;
}

void CNaviMeshAgent::SetAlwaysLookAt(const _bool _alwaysLookAt)
{
	m_bAlwaysLookAt = _alwaysLookAt;
}

const _bool CNaviMeshAgent::GetAlwaysLookAt() const
{
	return m_bAlwaysLookAt;
}

void CNaviMeshAgent::SetNavigationMeshResourceName(const wstring& _resourceName)
{
	if (m_strNavigationMeshResourceName == _resourceName)
		return;

	m_strNavigationMeshResourceName = _resourceName;
	m_bPathDirty = true;
	ClearRuntimePath();
}

const wstring& CNaviMeshAgent::GetNavigationMeshResourceName() const
{
	return m_strNavigationMeshResourceName;
}

const vector3& CNaviMeshAgent::GetResolvedDestination() const
{
	return m_vResolvedDestination;
}

const _int CNaviMeshAgent::GetCurrentPolygonIndex() const
{
	return m_iCurrentPolygonIndex;
}

const _int CNaviMeshAgent::GetPathPointCount() const
{
	return static_cast<_int>(m_vPathPoints.size());
}

const CNaviMeshAgent::CollisionWeight CNaviMeshAgent::GetCollisionWeight() const
{
	return m_eCollisionWeight;
}

void CNaviMeshAgent::SetCollisionWeight(const CollisionWeight _weight)
{
	m_eCollisionWeight = _weight;
}

EngineAI::CNaviMesh* CNaviMeshAgent::ResolveNavigationMesh() const
{
	CScene* scene = m_pGameObject ? m_pGameObject->Get_Scene() : nullptr;
	if (!scene)
		return nullptr;

	auto tryFindNavMesh = [&](const wstring& resourceName) -> EngineAI::CNaviMesh*
	{
		if (resourceName.empty())
			return nullptr;

		EngineAI::CNaviMesh* navMesh = dynamic_cast<EngineAI::CNaviMesh*>(scene->Find_Resource(resourceName));
		if (!navMesh || !navMesh->IsBuilt())
			return nullptr;

		return navMesh;
	};

	if (EngineAI::CNaviMesh* navMesh = tryFindNavMesh(m_strNavigationMeshResourceName))
		return navMesh;

	const wstring defaultResourceName = BuildDefaultNavigationMeshResourceName();
	if (EngineAI::CNaviMesh* navMesh = tryFindNavMesh(defaultResourceName))
		return navMesh;

	if (defaultResourceName != L"NavigationMesh")
		return tryFindNavMesh(L"NavigationMesh");

	return nullptr;
}

wstring CNaviMeshAgent::BuildDefaultNavigationMeshResourceName() const
{
	CScene* scene = m_pGameObject ? m_pGameObject->Get_Scene() : nullptr;
	if (!scene)
		return L"NavigationMesh";

	const wstring& sceneName = scene->Get_SceneName();
	if (sceneName.empty())
		return L"NavigationMesh";

	return sceneName + L" (NavigationMesh)";
}

vector3 CNaviMeshAgent::GetNavigationBaseOffset() const
{
    CTransform* transform = m_pGameObject ? m_pGameObject->GetTransform() : nullptr;
    if (!transform)
        return vector3(0.f, GetWorldAgentHeight() * 0.5f, 0.f);

    const vector3 scale = transform->Get_LocalScale();
    const vector3 scaledCenter(m_vAgentCenter.x * scale.x, m_vAgentCenter.y * scale.y, m_vAgentCenter.z * scale.z);
    return vector3(-scaledCenter.x, GetWorldAgentHeight() * 0.5f - scaledCenter.y, -scaledCenter.z);
}

_float CNaviMeshAgent::GetWorldAgentRadius() const
{
	CTransform* transform = m_pGameObject ? m_pGameObject->GetTransform() : nullptr;
	if (!transform)
		return max(0.05f, m_fAgentRadius);

	const vector3 scale = transform->Get_LocalScale();
	const _float radialScale = max(fabsf(scale.x), fabsf(scale.z));
	_float worldRadius = max(m_fAgentRadius * radialScale, 0.05f);

	if (m_pGameObject)
	{
		if (CCapsuleCollider* capsuleCollider = m_pGameObject->GetComponent<CCapsuleCollider>())
			worldRadius = max(worldRadius, max(capsuleCollider->GetRadius() * radialScale, 0.05f));

		if (CSphereCollider* sphereCollider = m_pGameObject->GetComponent<CSphereCollider>())
			worldRadius = max(worldRadius, max(sphereCollider->GetRadius() * radialScale, 0.05f));
	}

	return worldRadius;
}

_float CNaviMeshAgent::GetWorldAgentHeight() const
{
	CTransform* transform = m_pGameObject ? m_pGameObject->GetTransform() : nullptr;
	if (!transform)
		return max(0.05f, m_fAgentHeight);

	const vector3 scale = transform->Get_LocalScale();
	return max(m_fAgentHeight * fabsf(scale.y), 0.05f);
}

_bool CNaviMeshAgent::IsActivelyMovingForCollision() const
{
	if (!m_bHasDestination || !m_bOnNavigation)
		return false;

	if (m_fMoveSpeed <= kAgentPositionEpsilon)
		return false;

	return m_bHasPath || m_bPathDirty;
}

vector3 CNaviMeshAgent::ComputeSeparationOffset(const vector3& _currentNavigationPosition) const
{
	if (!m_pGameObject || !Get_Enable() || !m_pGameObject->IsRecursiveActive())
		return vector3::zero();

	const _float selfRadius = GetWorldAgentRadius();
	const _float selfHeight = GetWorldAgentHeight();
	const _int selfCollisionPriority = GetCollisionWeightPriority(m_eCollisionWeight);
	const _bool selfIsMoving = IsActivelyMovingForCollision();
	vector3 separation = vector3::zero();
	_uint overlapCount = 0u;

	for (CNaviMeshAgent* other : g_vActiveNavigationAgents)
	{
		if (!other || other == this || !other->Get_Enable() || !other->m_bOnNavigation)
			continue;

		CGameObject* otherObject = other->Get_GameObject();
		CTransform* otherTransform = other->GetTransform();
		if (!otherObject || !otherTransform || !otherObject->IsRecursiveActive())
			continue;

		if (otherObject->Get_Scene() != m_pGameObject->Get_Scene())
			continue;

		const vector3 otherGroundingOffset = other->GetNavigationBaseOffset() - vector3(0.f, other->m_fGroundSnapOffset, 0.f);
		const vector3 otherNavigationPosition = other->m_bHasLastNavigationPosition
			? other->m_vLastNavigationPosition
			: (otherTransform->Get_Position() - otherGroundingOffset);
		const _float otherHeight = other->GetWorldAgentHeight();
		const _float verticalOverlapThreshold = (selfHeight + otherHeight) * 0.5f;
		if (fabsf(_currentNavigationPosition.y - otherNavigationPosition.y) > verticalOverlapThreshold)
			continue;

		vector3 delta = _currentNavigationPosition - otherNavigationPosition;
		delta.y = 0.f;

		const _float minDistance = selfRadius + other->GetWorldAgentRadius() + kAgentSeparationPadding;
		const _float distanceSq = delta.lengthSq();
		if (distanceSq >= (minDistance * minDistance))
			continue;

		_float distance = 0.f;
		vector3 pushDirection = vector3::zero();
		if (distanceSq > (kAgentSeparationDirectionEpsilon * kAgentSeparationDirectionEpsilon))
		{
			distance = sqrtf(distanceSq);
			pushDirection = delta / distance;
		}
		else
		{
			const _float phase = static_cast<_float>((m_pGameObject->Get_UniqueID() ^ otherObject->Get_UniqueID()) & 1023u);
			const _float angle = (phase / 1024.f) * XM_2PI;
			pushDirection = vector3(cosf(angle), 0.f, sinf(angle));
			distance = 0.f;
		}

		const _float penetration = minDistance - distance;
		if (penetration <= 0.f)
			continue;

		const _int otherCollisionPriority = GetCollisionWeightPriority(other->GetCollisionWeight());
		_float selfDisplacementShare = 0.5f;

		if (selfCollisionPriority < otherCollisionPriority)
		{
			selfDisplacementShare = kAgentSeparationMaxShare;
		}
		else if (selfCollisionPriority > otherCollisionPriority)
		{
			selfDisplacementShare = kAgentSeparationMinShare;
		}
		else
		{
			const _bool otherIsMoving = other->IsActivelyMovingForCollision();
			if (selfIsMoving != otherIsMoving)
				selfDisplacementShare = selfIsMoving ? kAgentSeparationMovingShare : (1.f - kAgentSeparationMovingShare);
		}

		if (selfDisplacementShare <= 0.f)
			continue;

		separation += pushDirection * (penetration * selfDisplacementShare);
		++overlapCount;
	}

	if (overlapCount == 0u)
		return vector3::zero();

	separation /= static_cast<_float>(overlapCount);

	const _float maxSeparationStep = max(0.05f, selfRadius);
	const _float separationLength = separation.length();
	if (separationLength > maxSeparationStep && separationLength > kAgentSeparationDirectionEpsilon)
		separation = (separation / separationLength) * maxSeparationStep;

	return separation;
}

void CNaviMeshAgent::ApplySeparationOnNavigation(EngineAI::CNaviMesh* _navMesh, const vector3& _worldGroundingOffset)
{
	CTransform* transform = GetTransform();
	if (!transform || !_navMesh || !m_bOnNavigation || !m_bHasLastNavigationPosition)
		return;

	const vector3 currentNavigationPosition = m_vLastNavigationPosition;
	const vector3 separationOffset = ComputeSeparationOffset(currentNavigationPosition);
	if (separationOffset.lengthSq() <= 0.f)
		return;

	vector3 separatedNavigationPosition = currentNavigationPosition + separationOffset;
	_int separatedPolygonIndex = -1;
	_bool constrained = _navMesh->ConstrainMovement(currentNavigationPosition, separatedNavigationPosition, separatedNavigationPosition, &separatedPolygonIndex);
	if (!constrained || (separatedNavigationPosition - currentNavigationPosition).lengthSq() <= (kAgentPositionEpsilon * kAgentPositionEpsilon))
	{
		separatedNavigationPosition = currentNavigationPosition + separationOffset;
		constrained = SnapToNavigation(_navMesh, separatedNavigationPosition, separatedNavigationPosition, &separatedPolygonIndex);
	}

	if (!constrained)
		return;

	const vector3 separatedWorldPosition = separatedNavigationPosition + _worldGroundingOffset;
	if (vector3::Distance(transform->Get_Position(), separatedWorldPosition) > kAgentPositionEpsilon)
		transform->Set_Position(separatedWorldPosition);

	m_iCurrentPolygonIndex = separatedPolygonIndex;
	m_vLastNavigationPosition = separatedNavigationPosition;
	m_bHasLastNavigationPosition = true;
}

_bool CNaviMeshAgent::ConstrainTransformToNavigation(EngineAI::CNaviMesh* _navMesh, const vector3& _worldGroundingOffset, const _bool _preferSurfaceMoveFromLastPosition, const _bool _clearPathOnFailure)
{
	CTransform* transform = GetTransform();
	if (!transform || !_navMesh)
		return false;

	const vector3 currentNavigationPosition = transform->Get_Position() - _worldGroundingOffset;
	vector3 constrainedNavigationPosition = currentNavigationPosition;
	_int constrainedPolygonIndex = -1;

	_bool constrained = false;
	if (_preferSurfaceMoveFromLastPosition && m_bHasLastNavigationPosition)
	{
		constrained = _navMesh->ConstrainMovement(
			m_vLastNavigationPosition,
			currentNavigationPosition,
			constrainedNavigationPosition,
			&constrainedPolygonIndex);
	}

	if (!constrained)
	{
		constrained = SnapToNavigation(_navMesh, currentNavigationPosition, constrainedNavigationPosition, &constrainedPolygonIndex);
	}

	if (!constrained)
	{
		m_bOnNavigation = false;
		m_iCurrentPolygonIndex = -1;
		m_bHasLastNavigationPosition = false;
		if (_clearPathOnFailure)
		{
			ClearRuntimePath();
			m_bPathDirty = m_bHasDestination;
		}
		return false;
	}

	const vector3 constrainedWorldPosition = constrainedNavigationPosition + _worldGroundingOffset;
	if (vector3::Distance(transform->Get_Position(), constrainedWorldPosition) > kAgentPositionEpsilon)
		transform->Set_Position(constrainedWorldPosition);

	m_bOnNavigation = true;
	m_iCurrentPolygonIndex = constrainedPolygonIndex;
	m_vLastNavigationPosition = constrainedNavigationPosition;
	m_bHasLastNavigationPosition = true;
	return true;
}

_bool CNaviMeshAgent::SnapToNavigation(EngineAI::CNaviMesh* _navMesh, const vector3& _desiredPosition, vector3& _outPosition, _int* _outPolygonIndex) const
{
	if (_outPolygonIndex)
		*_outPolygonIndex = -1;

	if (!_navMesh)
		return false;

	_int polygonIndex = -1;
	if (!_navMesh->SamplePosition(_desiredPosition, _outPosition, &polygonIndex))
		return false;

	if (_outPolygonIndex)
		*_outPolygonIndex = polygonIndex;

	return true;
}

void CNaviMeshAgent::RebuildPath(EngineAI::CNaviMesh* _navMesh, const vector3& _currentPosition, const vector3& _destinationOnNavigation)
{
	ClearRuntimePath();
	m_vResolvedDestination = _destinationOnNavigation;

	if (!_navMesh)
	{
		m_bPathDirty = true;
		return;
	}

	vector<vector3> newPath = {};
	if (!_navMesh->FindPath(_currentPosition, _destinationOnNavigation, newPath) || newPath.empty())
	{
		m_bPathDirty = true;
		return;
	}

	m_vPathPoints = move(newPath);
	m_iPathIndex = 0;
	m_bHasPath = true;
	m_bPathDirty = false;
}

void CNaviMeshAgent::ClearRuntimePath()
{
	m_vPathPoints.clear();
	m_iPathIndex = 0;
	m_bHasPath = false;
}

