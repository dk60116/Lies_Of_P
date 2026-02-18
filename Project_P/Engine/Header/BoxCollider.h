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

	const vector3& GetSize() const;

	void OnDestroy() override;

protected:
	void BuildShapeIfNeeded() override;

private:
	vector3 m_vSize;
};

NS_END

