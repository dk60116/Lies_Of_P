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
	void Update_Editor() override;
	void Render_Gizmo() override;
	void OnDestroy() override;

public:
	void SetParent(CTransform* _parent) override;
	void Set_LocalScale(const vector3& _scale);
	void Set_LocalScale(const _float _x, const _float _y, const _float _z);
	void Set_LocalScale(const _float _value);
	void Set_LocalScaleX(const _float _value);
	void Set_LocalScaleY(const _float _value);
	void Set_LocalScaleZ(const _float _value);
	void Add_LocalScale(const vector3& _scale);
	void Add_LocalScaleX(const _float _value);
	void Add_LocalScaleY(const _float _value);
	void Add_LocalScaleZ(const _float _value);

	const vector2 Get_AnchoredPosition() const;
	void Set_AnchoredPosition(const vector2 _pos);
	void Set_AnchoredPosition(const _float _x, const _float _y);
	void Set_AnchoredPositonX(const _float _value);
	void Set_AnchoredPositonY(const _float _value);
	const vector2 Get_AnchoredSize() const;
	void Set_AnchoredSize(const vector2 _size);
	void Set_AnchoredSize(const _float _x, const _float _y);
	const vector3 Get_SizeScale() const;
	void Set_SizeScale(const vector3& _scale);
	void Set_SizeScale(const _float _x, const _float _y, const _float _z);
	const vector2 Get_ScreenPosition() const;
	const vector2 Get_ScreenCenterPosition() const;
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
	vector2 GetReferenceSize() const;
	vector2 GetAnchorSpan() const;
	vector2 GetAnchorReference() const;
	void RefreshSizeFromLayout();
	void SyncAnchoredPositionFromLocal();
	void SyncLocalPositionFromAnchored();
	_bool NeedsDefaultLayoutInitialization() const;

private:
	CUI* m_pUI;
	vector2 m_vAnchoredPosition, m_vAnchoredScale;
	vector2 m_vStaticWH;
	vector3 m_vSizeScale;
	_float m_fWidth, m_fHeight;

	Anchors m_sAnchors;
	vector2 m_vPivot;

	CRectTransform* m_pParentRect;
	_bool m_bIsRootRect;
};

NS_END

