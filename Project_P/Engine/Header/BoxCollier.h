#pragma once
#include "Collider.h"

NS_BEGIN(Engine)

class CBoxCollier : public CCollider
{
public:
protected:
	explicit CBoxCollier();
	~CBoxCollier();

private:
	static CBoxCollier* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Update() override;
	void FixedUpdate() override;
	void Render_Editor() override;

	void OnDestroy() override;

protected:
	void BuildShapeIfNeeded() override;

private:
	vector3 m_vSize;
};

NS_END

