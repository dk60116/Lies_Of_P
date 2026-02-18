#pragma once
#include "Collider.h"

NS_BEGIN(Engine)

class ENGINE_DLL CBoxCollider : public CCollider
{
protected:
	explicit CBoxCollider();
	~CBoxCollider();

public:
	static CBoxCollider* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Update() override;
	void FixedUpdate() override;
	void Render_Editor() override;
	void Render_Gizmo() override;

	const vector3& GetSize() const;
	void SetSize(const vector3& size);

	void OnDestroy() override;

protected:
	void BuildShapeIfNeeded() override;

private:
	vector3 m_vSize;
	class CMeshBuffer* m_pLineMesh;
	class CMaterial* m_pLineMaterial;
};

NS_END

