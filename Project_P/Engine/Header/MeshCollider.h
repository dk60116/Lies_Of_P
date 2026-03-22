#pragma once

#include "Collider.h"

NS_BEGIN(Engine)

class ENGINE_DLL CMeshCollider final : public CCollider
{
    friend class CGameObject;

protected:
    explicit CMeshCollider();
    ~CMeshCollider();

private:
    static CMeshCollider* Create();
    CComponent* Clone() const override;

public:
    HRESULT Initialize() override;
    void Update() override;
    void FixedUpdate() override;
    void Render_Editor() override;
    void Render_Gizmo() override;

    void OnDestroy() override;

public:
    void BuildShapeIfNeeded() override;
    const _bool IsGizmoVisible() const;
    void SetGizmoVisible(const _bool visible);

private:
    class CMeshBuffer* m_pCachedMeshBuffer;
    class CMaterial* m_pLineMaterial;
    _bool m_bShowGizmo;
    vector3 m_vCachedWorldScale;
};

NS_END
