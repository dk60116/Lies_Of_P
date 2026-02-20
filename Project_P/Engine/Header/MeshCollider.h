#pragma once

#include "Collider.h"

NS_BEGIN(Engine)

class ENGINE_DLL CMeshCollider final : public CCollider
{
protected:
    explicit CMeshCollider();
    ~CMeshCollider();

public:
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

private:
    class CMeshBuffer* m_pCachedMeshBuffer;
    class CMeshBuffer* m_pLineMesh;
    class CMaterial* m_pLineMaterial;
};

NS_END
