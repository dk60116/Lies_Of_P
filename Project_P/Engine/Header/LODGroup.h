#pragma once

#include "epch.h"
#include "Component.h"

NS_BEGIN(Engine)

class CCamera;
class CRenderer;

class ENGINE_DLL CLODGroup final : public CComponent
{
	friend class CGameObject;

public:
	struct LODLevel
	{
		_int lodIndex = 0;
		vector<CRenderer*> renderers = {};
	};

protected:
	explicit CLODGroup();
	~CLODGroup();

public:
	static CLODGroup* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void LateUpdate_Editor() override;
	void LateUpdate() override;
	void OnPreCull() override;
	void OnPreRender() override;
	void Render_Editor() override;
	void Render() override;
	void OnPostRender() override;
	void OnEnable() override;
	void OnDisable() override;
	void OnDestroy() override;

public:
	void RefreshLODLevels();
	void MarkRefreshNeeded();
	const vector<_float>& GetSwitchDistances() const;
	void SetSwitchDistances(const vector<_float>& _distances);
	void SetSwitchDistance(const _uint _index, const _float _distance);
	_uint GetLODLevelCount() const;
	_int GetCurrentLODIndex() const;
	_int GetLODSourceIndex(const _uint _index) const;
	_uint GetLODRendererCount(const _uint _index) const;
	void EvaluateLODForCamera(CCamera* _camera);

private:
	void UpdateLODForCamera(CCamera* _camera);
	_uint PickLODLevelIndex(const _float _distanceSq) const;
	void ApplyLODLevel(const _int _levelIndex);
	void SetAllManagedRenderersVisible(const _bool _visible);
	void ClearLODLevels(const _bool _restoreVisibility);
	void NormalizeSwitchDistances();
	void RebuildMeshBounds();
	_bool TryBuildWorldMeshBounds(BoundingBox& _outWorldBounds);

private:
	vector<LODLevel> m_vLODLevels;
	vector<_float> m_vSwitchDistances;
	BoundingBox m_sMeshBoundsLocal;
	_int m_iCurrentLODIndex;
	_bool m_bRefreshNeeded;
	_bool m_bHasMeshBounds;
};

NS_END

