#pragma once

#include "Transform.h"

NS_BEGIN(Engine)

class ENGINE_DLL CRectTransform final : public CTransform
{
public:
	struct Anchors
	{
		vector2 min = vector2::one() * 0.5f;
		vector2 max = vector2::one() * 0.5f;
	};

	friend class CGameObject;
	friend class CUI;

protected:
	CRectTransform();
	~CRectTransform();

private:
	static CRectTransform* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Update() override;
	void Render_Gizmo() override;
	void OnDestroy() override;

public:
	void SetParent(CTransform* _parent) override;

	const vector2 Get_AnchoredPosition() const;
	void Set_AnchoredPosition(const vector2 _pos);
	void Set_AnchoredPosition(const _float _x, const _float _y);
	void Set_AnchoredPositonX(const _float _value);
	void Set_AnchoredPositonY(const _float _value);
	const vector2 Get_ScreenPosition() const;
	const _float Get_Width() const;
	const _float Get_Height() const;
	const vector2 Get_WidthHeight() const;
	const vector2 Get_Pivot() const;
	void Set_Pivot(vector2 _pivot);
	void Set_Pivot(const _float _x, const _float _y);
	void Set_PivotX(_float _value);
	void Set_PivotY(_float _value);
	const Anchors& Get_Anchors();
	void Set_AnchorsMin(const vector2 _pivot);
	void Set_AnchorsMin(const _float _x, const _float _y);
	void Set_AnchorsMax(const vector2 _pivot);
	void Set_AnchorsMax(const _float _x, const _float _y);

public:
	void Set_WidthHeight(const vector2 _rect);
	void Set_WidthHeight(const _float _x, const _float _y);
	void Set_WidthHeight(const _int _x, const _int _y);
	void Set_WidthHeight(const _int _wh);
	void Set_Width(const _float _value);
	void Set_Width(const _int _value);
	void Set_Height(const _float _value);
	void Set_Height(const _int _value);

private:
	void Set_UI(class CUI* _pUI);

private:
	CUI* m_pUI;
	vector2 m_vAnchoredPosition, m_vAnchoredScale;
	vector2 m_vStaticWH;
	_float m_fWidth, m_fHeight;

	Anchors m_sAnchors;
	vector2 m_vPivot;

	CRectTransform* m_pParentRect;
	_bool m_bIsRootRect;
};

NS_END

