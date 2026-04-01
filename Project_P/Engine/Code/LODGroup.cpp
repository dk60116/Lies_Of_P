#include "epch.h"
#include "LODGroup.h"
#include "Camera.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "Renderer.h"
#include "Scene.h"
#include "Transform.h"
#include <algorithm>
#include <cwctype>
#include <map>

namespace
{
	wstring ToLowerCopy(wstring _value)
	{
		transform
		(
			_value.begin(),
			_value.end(),
			_value.begin(),
			[](wchar_t ch)
			{
				return static_cast<wchar_t>(towlower(ch));
			}
		);

		return _value;
	}

	_bool TryExtractLODIndexFromName(const wstring& _name, _int& _outIndex)
	{
		const wstring lowerName = ToLowerCopy(_name);
		size_t searchOffset = 0u;

		while (searchOffset < lowerName.size())
		{
			const size_t lodPos = lowerName.find(L"lod", searchOffset);
			if (lodPos == wstring::npos)
				return false;

			size_t digitPos = lodPos + 3u;
			while (digitPos < lowerName.size())
			{
				const wchar_t token = lowerName[digitPos];
				if (token == L'_' || token == L'-' || token == L' ')
				{
					++digitPos;
					continue;
				}
				break;
			}

			if (digitPos < lowerName.size() && iswdigit(lowerName[digitPos]))
			{
				_int lodIndex = 0;
				while (digitPos < lowerName.size() && iswdigit(lowerName[digitPos]))
				{
					lodIndex = lodIndex * 10 + static_cast<_int>(lowerName[digitPos] - L'0');
					++digitPos;
				}

				_outIndex = lodIndex;
				return true;
			}

			searchOffset = lodPos + 3u;
		}

		return false;
	}

	_float GetDefaultLODTransitionDistance(const _uint _index)
	{
		static constexpr _float s_fDefaultDistances[] = { 20.f, 50.f, 100.f, 180.f, 300.f };

		if (_index < _countof(s_fDefaultDistances))
			return s_fDefaultDistances[_index];

		const _uint overflowIndex = _index - static_cast<_uint>(_countof(s_fDefaultDistances)) + 1u;
		return s_fDefaultDistances[_countof(s_fDefaultDistances) - 1u] + 120.f * static_cast<_float>(overflowIndex);
	}

	void CollectLODRenderersRecursive(CTransform* _transform, CTransform* _groupRoot, _int _inheritedLODIndex, map<_int, vector<CRenderer*>>& _outLevels)
	{
		if (!_transform)
			return;

		CGameObject* owner = _transform->Get_GameObject();
		if (!owner)
			return;

		if (_transform != _groupRoot && owner->GetComponent<CLODGroup>())
			return;

		_int currentLODIndex = max(0, _inheritedLODIndex);
		_int parsedLODIndex = 0;
		if (TryExtractLODIndexFromName(owner->Get_ObjectName(), parsedLODIndex))
			currentLODIndex = max(0, parsedLODIndex);

		for (CComponent* component : owner->Get_ComponentList())
		{
			if (CRenderer* renderer = dynamic_cast<CRenderer*>(component))
				_outLevels[currentLODIndex].push_back(renderer);
		}

		for (CTransform* child : _transform->Get_ChldList())
			CollectLODRenderersRecursive(child, _groupRoot, currentLODIndex, _outLevels);
	}

	_bool TryBuildMeshRendererBoundsInGroupSpace(CMeshRenderer* _renderer, const _matrix& _rendererToGroup, BoundingBox& _outBounds)
	{
		if (!_renderer)
			return false;

		CMeshBuffer* meshBuffer = _renderer->Get_MeshBuffer();
		if (!meshBuffer)
			return false;

		const BoundingBox& localBounds = meshBuffer->Get_Info().boundingBox;
		XMFLOAT3 corners[BoundingBox::CORNER_COUNT] = {};
		localBounds.GetCorners(corners);

		_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.f);
		_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.f);

		for (const XMFLOAT3& corner : corners)
		{
			const _vector local = XMVectorSet(corner.x, corner.y, corner.z, 1.f);
			const _vector groupSpace = XMVector3TransformCoord(local, _rendererToGroup);
			minV = XMVectorMin(minV, groupSpace);
			maxV = XMVectorMax(maxV, groupSpace);
		}

		BoundingBox::CreateFromPoints(_outBounds, minV, maxV);
		return true;
	}

	_float ComputeDistanceSqToBoundingBox(const vector3& _point, const BoundingBox& _bounds)
	{
		const auto clampFloat = [](_float _value, _float _minValue, _float _maxValue)
		{
			return (_value < _minValue) ? _minValue : ((_value > _maxValue) ? _maxValue : _value);
		};

		const _float3 minBound =
		{
			_bounds.Center.x - _bounds.Extents.x,
			_bounds.Center.y - _bounds.Extents.y,
			_bounds.Center.z - _bounds.Extents.z
		};
		const _float3 maxBound =
		{
			_bounds.Center.x + _bounds.Extents.x,
			_bounds.Center.y + _bounds.Extents.y,
			_bounds.Center.z + _bounds.Extents.z
		};

		const vector3 closestPoint
		(
			clampFloat(_point.x, minBound.x, maxBound.x),
			clampFloat(_point.y, minBound.y, maxBound.y),
			clampFloat(_point.z, minBound.z, maxBound.z)
		);

		return (_point - closestPoint).lengthSq();
	}
}

CLODGroup::CLODGroup()
	: m_vLODLevels({})
	, m_vSwitchDistances({})
	, m_sMeshBoundsLocal({})
	, m_iCurrentLODIndex(-1)
	, m_bRefreshNeeded(true)
	, m_bHasMeshBounds(false)
{
	m_strName = L"LODGroup";
}

CLODGroup::~CLODGroup()
{
}

CLODGroup* CLODGroup::Create()
{
	return new CLODGroup();
}

CComponent* CLODGroup::Clone() const
{
	CLODGroup* clone = new CLODGroup();
	clone->m_vSwitchDistances = m_vSwitchDistances;

	return clone;
}

HRESULT CLODGroup::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	m_bRefreshNeeded = true;
	return S_OK;
}

void CLODGroup::Awake()
{
	RefreshLODLevels();
}

void CLODGroup::LateUpdate_Editor()
{
	if (m_bRefreshNeeded)
		RefreshLODLevels();

	CScene* scene = Get_GameObject() ? Get_GameObject()->Get_Scene() : nullptr;
	UpdateLODForCamera(scene ? scene->Get_EditorCamera() : nullptr);
}

void CLODGroup::LateUpdate()
{
	if (m_bRefreshNeeded)
		RefreshLODLevels();

	CScene* scene = Get_GameObject() ? Get_GameObject()->Get_Scene() : nullptr;
	UpdateLODForCamera(scene ? scene->Get_Camera() : nullptr);
}

void CLODGroup::OnPreCull()
{
}

void CLODGroup::OnPreRender()
{
}

void CLODGroup::Render_Editor()
{
}

void CLODGroup::Render()
{
}

void CLODGroup::OnPostRender()
{
}

void CLODGroup::OnEnable()
{
	m_bRefreshNeeded = true;
}

void CLODGroup::OnDisable()
{
	SetAllManagedRenderersVisible(true);
	m_iCurrentLODIndex = -1;
	m_bRefreshNeeded = true;
}

void CLODGroup::OnDestroy()
{
	ClearLODLevels(true);
	m_bRefreshNeeded = false;
}

void CLODGroup::RefreshLODLevels()
{
	m_bRefreshNeeded = false;
	ClearLODLevels(true);
	m_iCurrentLODIndex = -1;

	CTransform* rootTransform = GetTransform();
	if (!rootTransform)
	{
		m_iCurrentLODIndex = -1;
		return;
	}

	_int rootLODIndex = 0;
	_int parsedRootLODIndex = 0;
	if (Get_GameObject() && TryExtractLODIndexFromName(Get_GameObject()->Get_ObjectName(), parsedRootLODIndex))
		rootLODIndex = max(0, parsedRootLODIndex);

	map<_int, vector<CRenderer*>> collectedLevels = {};
	CollectLODRenderersRecursive(rootTransform, rootTransform, rootLODIndex, collectedLevels);

	m_vLODLevels.reserve(collectedLevels.size());

	for (auto& [lodIndex, renderers] : collectedLevels)
	{
		if (renderers.empty())
			continue;

		LODLevel level = {};
		level.lodIndex = max(0, lodIndex);
		level.renderers.reserve(renderers.size());

		for (CRenderer* renderer : renderers)
		{
			if (!renderer)
				continue;

			renderer->AddRef();
			level.renderers.push_back(renderer);
		}

		if (!level.renderers.empty())
			m_vLODLevels.push_back(move(level));
	}

	NormalizeSwitchDistances();
	RebuildMeshBounds();

	if (m_vLODLevels.empty())
		m_iCurrentLODIndex = -1;
}

void CLODGroup::MarkRefreshNeeded()
{
	m_bRefreshNeeded = true;
}

const vector<_float>& CLODGroup::GetSwitchDistances() const
{
	return m_vSwitchDistances;
}

void CLODGroup::SetSwitchDistances(const vector<_float>& _distances)
{
	m_vSwitchDistances = _distances;
	NormalizeSwitchDistances();
}

void CLODGroup::SetSwitchDistance(const _uint _index, const _float _distance)
{
	if (m_vSwitchDistances.size() <= _index)
		m_vSwitchDistances.resize(static_cast<size_t>(_index) + 1u, GetDefaultLODTransitionDistance(_index));

	m_vSwitchDistances[_index] = max(0.f, _distance);
	NormalizeSwitchDistances();
}

_uint CLODGroup::GetLODLevelCount() const
{
	return static_cast<_uint>(m_vLODLevels.size());
}

_int CLODGroup::GetCurrentLODIndex() const
{
	return m_iCurrentLODIndex;
}

_int CLODGroup::GetLODSourceIndex(const _uint _index) const
{
	if (_index >= m_vLODLevels.size())
		return -1;

	return m_vLODLevels[_index].lodIndex;
}

_uint CLODGroup::GetLODRendererCount(const _uint _index) const
{
	if (_index >= m_vLODLevels.size())
		return 0u;

	return static_cast<_uint>(m_vLODLevels[_index].renderers.size());
}

void CLODGroup::EvaluateLODForCamera(CCamera* _camera)
{
	UpdateLODForCamera(_camera);
}

void CLODGroup::UpdateLODForCamera(CCamera* _camera)
{
	if (m_vLODLevels.empty())
	{
		m_iCurrentLODIndex = -1;
		return;
	}

	if (!_camera || !_camera->GetTransform())
	{
		SetAllManagedRenderersVisible(true);
		m_iCurrentLODIndex = -1;
		return;
	}

	const vector3 cameraPosition = _camera->GetTransform()->Get_Position();
	_float distanceSq = 0.f;

	BoundingBox worldBounds = {};
	if (TryBuildWorldMeshBounds(worldBounds))
		distanceSq = ComputeDistanceSqToBoundingBox(cameraPosition, worldBounds);
	else
	{
		const vector3 groupPosition = GetTransform() ? GetTransform()->Get_Position() : vector3::zero();
		distanceSq = (cameraPosition - groupPosition).lengthSq();
	}

	ApplyLODLevel(static_cast<_int>(PickLODLevelIndex(distanceSq)));
}

_uint CLODGroup::PickLODLevelIndex(const _float _distanceSq) const
{
	if (m_vLODLevels.size() <= 1u)
		return 0u;

	for (_uint transitionIndex = 0u; transitionIndex < static_cast<_uint>(m_vSwitchDistances.size()); ++transitionIndex)
	{
		const _float switchDistance = max(0.f, m_vSwitchDistances[transitionIndex]);
		if (_distanceSq < switchDistance * switchDistance)
			return transitionIndex;
	}

	return static_cast<_uint>(m_vLODLevels.size() - 1u);
}

void CLODGroup::ApplyLODLevel(const _int _levelIndex)
{
	if (m_vLODLevels.empty())
	{
		m_iCurrentLODIndex = -1;
		return;
	}

	const _int clampedLevelIndex = clamp(_levelIndex, 0, static_cast<_int>(m_vLODLevels.size()) - 1);
	if (m_iCurrentLODIndex == clampedLevelIndex)
		return;

	m_iCurrentLODIndex = clampedLevelIndex;

	for (_uint levelIndex = 0u; levelIndex < static_cast<_uint>(m_vLODLevels.size()); ++levelIndex)
	{
		const _bool shouldBeVisible = static_cast<_int>(levelIndex) == clampedLevelIndex;
		for (CRenderer* renderer : m_vLODLevels[levelIndex].renderers)
		{
			if (renderer)
				renderer->SetLODVisible(shouldBeVisible);
		}
	}
}

void CLODGroup::SetAllManagedRenderersVisible(const _bool _visible)
{
	for (LODLevel& level : m_vLODLevels)
	{
		for (CRenderer* renderer : level.renderers)
		{
			if (renderer)
				renderer->SetLODVisible(_visible);
		}
	}
}

void CLODGroup::ClearLODLevels(const _bool _restoreVisibility)
{
	for (LODLevel& level : m_vLODLevels)
	{
		for (CRenderer* renderer : level.renderers)
		{
			if (_restoreVisibility && renderer)
				renderer->SetLODVisible(true);

			Safe_Release(renderer);
		}

		level.renderers.clear();
	}

	m_vLODLevels.clear();
	m_bHasMeshBounds = false;
	m_sMeshBoundsLocal = {};
}

void CLODGroup::NormalizeSwitchDistances()
{
	const size_t requiredTransitionCount = m_vLODLevels.empty() ? m_vSwitchDistances.size() : (m_vLODLevels.size() - 1u);

	if (m_vSwitchDistances.size() < requiredTransitionCount)
	{
		const size_t prevSize = m_vSwitchDistances.size();
		m_vSwitchDistances.resize(requiredTransitionCount);
		for (size_t index = prevSize; index < requiredTransitionCount; ++index)
			m_vSwitchDistances[index] = GetDefaultLODTransitionDistance(static_cast<_uint>(index));
	}

	if (!m_vLODLevels.empty() && m_vSwitchDistances.size() > requiredTransitionCount)
		m_vSwitchDistances.resize(requiredTransitionCount);

	_float previousDistance = 0.f;
	for (size_t index = 0; index < m_vSwitchDistances.size(); ++index)
	{
		_float normalizedDistance = max(0.f, m_vSwitchDistances[index]);
		if (index > 0u)
			normalizedDistance = max(normalizedDistance, previousDistance + 0.01f);

		m_vSwitchDistances[index] = normalizedDistance;
		previousDistance = normalizedDistance;
	}
}

void CLODGroup::RebuildMeshBounds()
{
	m_bHasMeshBounds = false;
	m_sMeshBoundsLocal = {};

	CTransform* rootTransform = GetTransform();
	if (!rootTransform)
		return;

	const _matrix rootWorld = rootTransform->Get_WorldMatrix();
	const _matrix invRootWorld = XMMatrixInverse(nullptr, rootWorld);

	_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.f);
	_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.f);
	_bool hasBounds = false;

	for (const LODLevel& level : m_vLODLevels)
	{
		for (CRenderer* renderer : level.renderers)
		{
			CMeshRenderer* meshRenderer = dynamic_cast<CMeshRenderer*>(renderer);
			if (!meshRenderer || !meshRenderer->GetTransform())
				continue;

			const _matrix rendererWorld = meshRenderer->GetTransform()->Get_WorldMatrix();
			const _matrix rendererToGroup = rendererWorld * invRootWorld;

			BoundingBox rendererBounds = {};
			if (!TryBuildMeshRendererBoundsInGroupSpace(meshRenderer, rendererToGroup, rendererBounds))
				continue;

			XMFLOAT3 corners[BoundingBox::CORNER_COUNT] = {};
			rendererBounds.GetCorners(corners);

			for (const XMFLOAT3& corner : corners)
			{
				const _vector p = XMVectorSet(corner.x, corner.y, corner.z, 1.f);
				minV = XMVectorMin(minV, p);
				maxV = XMVectorMax(maxV, p);
			}

			hasBounds = true;
		}
	}

	if (!hasBounds)
		return;

	BoundingBox::CreateFromPoints(m_sMeshBoundsLocal, minV, maxV);
	m_bHasMeshBounds = true;
}

_bool CLODGroup::TryBuildWorldMeshBounds(BoundingBox& _outWorldBounds)
{
	CTransform* rootTransform = GetTransform();
	if (!m_bHasMeshBounds || !rootTransform)
		return false;

	BoundingOrientedBox localObb = {};
	BoundingOrientedBox::CreateFromBoundingBox(localObb, m_sMeshBoundsLocal);

	BoundingOrientedBox worldObb = {};
	const _matrix rootWorld = rootTransform->Get_WorldMatrix();
	localObb.Transform(worldObb, rootWorld);

	XMFLOAT3 corners[BoundingBox::CORNER_COUNT] = {};
	worldObb.GetCorners(corners);

	_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.f);
	_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.f);

	for (const XMFLOAT3& corner : corners)
	{
		const _vector p = XMVectorSet(corner.x, corner.y, corner.z, 1.f);
		minV = XMVectorMin(minV, p);
		maxV = XMVectorMax(maxV, p);
	}

	BoundingBox::CreateFromPoints(_outWorldBounds, minV, maxV);
	return true;
}
