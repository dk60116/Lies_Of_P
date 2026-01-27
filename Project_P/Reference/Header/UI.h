#pragma once

#include "Component.h"
#include "UIManager.h"

NS_BEGIN(Engine)

class ENGINE_DLL CUI : public CComponent
{
	friend class CGameObject;
	friend class CRectTransform;

protected:
	CUI();
	~CUI();

public:
	HRESULT Initialize() override;
	void OnDestroy();

private:
	static CUI* Create();
	CComponent* Clone() const override;

public:
	void Set_Mesh(CMeshBuffer* _mesh);
	void Set_Material(CMaterial* _material);

public:
	void Bind_Mesh();
	void Bind_Matrix();
	void Bind_Camera(const _fmatrix _view, const _cmatrix _projection);
	CRectTransform* Get_RectTransform() const;

public:
	CCanvas* Get_Canvas() const;
	void SetColor(ColorValue _color);
	void SetAlpha(const _float _value);

private:
	const _bool Is_Canvas() const;
	void Set_Canvas(class CCanvas* _canvas);

protected:
	CRectTransform* m_pRectTransform;
	class CCanvas* m_pCanvas;
	CMeshBuffer* m_pRectMesh;
	CMaterial* m_pMaterial;

	_bool m_bIsCanvas;

protected:
	CMeshBuffer* m_pRectGizmoMesh;
	CMaterial* m_pLineMat;

protected:
	ColorValue m_vColor;
};

NS_END

