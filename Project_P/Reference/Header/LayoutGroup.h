#pragma once
#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL CLayoutGroup abstract : public CComponent
{
public:
	enum class ChildAlignment
	{
		UpperLeft,
		UpperCenter,
		UpperRight,
		MiddleLeft,
		MiddleCenter,
		MiddleRight,
		LowerLeft,
		LowerCenter,
		LowerRight
	};

	struct Padding
	{
		_float left = 0.f;
		_float right = 0.f;
		_float top = 0.f;
		_float bottom = 0.f;
	};

protected:
	struct LayoutContext
	{
		class CRectTransform* parentRect = nullptr;
		vector<class CRectTransform*> children = {};
		_float parentWidth = 0.f;
		_float parentHeight = 0.f;
		_float innerWidth = 0.f;
		_float innerHeight = 0.f;
	};

protected:
	CLayoutGroup();
	~CLayoutGroup();

public:
	HRESULT Initialize() override;
	void LateUpdate() override;
	void LateUpdate_Editor() override;
	void OnDestroy() override;

public:
	const Padding& GetPadding() const;
	void SetPadding(const Padding& padding);
	void SetPadding(const _float left, const _float right, const _float top, const _float bottom);

	const _float GetSpacing() const;
	void SetSpacing(const _float spacing);

	const ChildAlignment GetChildAlignment() const;
	void SetChildAlignment(const ChildAlignment alignment);

	const _bool GetControlChildSizeWidth() const;
	const _bool GetControlChildSizeHeight() const;
	void SetControlChildSize(const _bool width, const _bool height);

	const _bool GetForceExpandWidth() const;
	const _bool GetForceExpandHeight() const;
	void SetForceExpand(const _bool width, const _bool height);

protected:
	void CopyLayoutSettingsTo(CLayoutGroup* target) const;
	_bool BuildLayoutContext(LayoutContext& outContext) const;
	_float GetHorizontalAlignmentFactor() const;
	_float GetVerticalAlignmentFactor() const;
	void SetChildLayout(class CRectTransform* child, const _float x, const _float y, const _float width, const _float height, const _bool setWidth, const _bool setHeight) const;

private:
	void ExecuteLayout();
	virtual void ApplyLayout(const LayoutContext& context) = 0;

protected:
	Padding m_tPadding;
	_float m_fSpacing;
	ChildAlignment m_eChildAlignment;
	_bool m_bControlChildSizeWidth;
	_bool m_bControlChildSizeHeight;
	_bool m_bForceExpandWidth;
	_bool m_bForceExpandHeight;

private:
	_bool m_bIsApplyingLayout;
};

NS_END

