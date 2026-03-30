#pragma once

#include "epch.h"
#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL CLODGroup final : public CComponent
{
	friend class CGameObject;

protected:
	explicit CLODGroup();
	~CLODGroup();

public:
	static CLODGroup* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Awake() override;
	void OnPreCull() override;
	void OnPreRender() override;
	void Render_Editor() override;
	void Render() override;
	void OnPostRender() override;
	void OnDestroy() override;

public:
	void InitializeMeshRenders();

private:
	vector<CMeshRenderer*> m_vRenderList;
};

NS_END

