#pragma once
#include "Collider.h"

NS_BEGIN(Engine)

class ENGINE_DLL CCapsuleCollider : public CCollider
{
    friend class CGameObject;

protected:
    explicit CCapsuleCollider();
    ~CCapsuleCollider();

private:
    static CCapsuleCollider* Create();
    CComponent* Clone() const override;

public:
    HRESULT Initialize() override;
    void Update() override;
    void FixedUpdate() override;
    void Render_Editor() override;
    void Render_Gizmo() override;

    const _float GetRadius() const;
    const _float GetHeight() const;
    void SetRadius(const _float radius);
    void SetHeight(const _float height);

    void OnDestroy() override;

public:
    void BuildShapeIfNeeded() override;

private:
    _float m_fRadius;
    _float m_fHeight;
    class CMeshBuffer* m_pLineMesh;
    class CMaterial* m_pLineMaterial;
};

NS_END
