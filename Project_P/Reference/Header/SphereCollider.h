#pragma once
#include "Collider.h"

NS_BEGIN(Engine)

class ENGINE_DLL CSphereCollider : public CCollider
{
protected:
    explicit CSphereCollider();
    ~CSphereCollider();

public:
    static CSphereCollider* Create();
    CComponent* Clone() const override;

public:
    HRESULT Initialize() override;
    void Update() override;
    void FixedUpdate() override;
    void Render_Editor() override;
    void Render_Gizmo() override;

    const _float GetRadius() const;
    void SetRadius(const _float radius);

    void OnDestroy() override;

public:
    void BuildShapeIfNeeded() override;

private:
    _float m_fRadius;
    class CMeshBuffer* m_pLineMesh;
    class CMaterial* m_pLineMaterial;
};

NS_END
