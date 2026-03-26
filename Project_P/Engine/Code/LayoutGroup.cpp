#include "epch.h"
#include "LayoutGroup.h"
#include "GameObject.h"
#include "RectTransform.h"

CLayoutGroup::CLayoutGroup()
	: m_tPadding({})
	, m_fSpacing(0.f)
	, m_eChildAlignment(ChildAlignment::UpperLeft)
	, m_bControlChildSizeWidth(false)
	, m_bControlChildSizeHeight(false)
	, m_bForceExpandWidth(false)
	, m_bForceExpandHeight(false)
	, m_bIsApplyingLayout(false)
{
	m_strName = L"Layout Group";
}

CLayoutGroup::~CLayoutGroup()
{
}

HRESULT CLayoutGroup::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	return S_OK;
}

void CLayoutGroup::LateUpdate()
{
	ExecuteLayout();
}

void CLayoutGroup::LateUpdate_Editor()
{
	ExecuteLayout();
}

void CLayoutGroup::OnDestroy()
{
}

const CLayoutGroup::Padding& CLayoutGroup::GetPadding() const
{
	return m_tPadding;
}

void CLayoutGroup::SetPadding(const Padding& padding)
{
	m_tPadding.left = max(0.f, padding.left);
	m_tPadding.right = max(0.f, padding.right);
	m_tPadding.top = max(0.f, padding.top);
	m_tPadding.bottom = max(0.f, padding.bottom);
}

void CLayoutGroup::SetPadding(const _float left, const _float right, const _float top, const _float bottom)
{
	SetPadding({ left, right, top, bottom });
}

const _float CLayoutGroup::GetSpacing() const
{
	return m_fSpacing;
}

void CLayoutGroup::SetSpacing(const _float spacing)
{
	m_fSpacing = max(0.f, spacing);
}

const CLayoutGroup::ChildAlignment CLayoutGroup::GetChildAlignment() const
{
	return m_eChildAlignment;
}

void CLayoutGroup::SetChildAlignment(const ChildAlignment alignment)
{
	m_eChildAlignment = alignment;
}

const _bool CLayoutGroup::GetControlChildSizeWidth() const
{
	return m_bControlChildSizeWidth;
}

const _bool CLayoutGroup::GetControlChildSizeHeight() const
{
	return m_bControlChildSizeHeight;
}

void CLayoutGroup::SetControlChildSize(const _bool width, const _bool height)
{
	m_bControlChildSizeWidth = width;
	m_bControlChildSizeHeight = height;
}

const _bool CLayoutGroup::GetForceExpandWidth() const
{
	return m_bForceExpandWidth;
}

const _bool CLayoutGroup::GetForceExpandHeight() const
{
	return m_bForceExpandHeight;
}

void CLayoutGroup::SetForceExpand(const _bool width, const _bool height)
{
	m_bForceExpandWidth = width;
	m_bForceExpandHeight = height;
}

void CLayoutGroup::CopyLayoutSettingsTo(CLayoutGroup* target) const
{
	if (!target)
		return;

	target->m_tPadding = m_tPadding;
	target->m_fSpacing = m_fSpacing;
	target->m_eChildAlignment = m_eChildAlignment;
	target->m_bControlChildSizeWidth = m_bControlChildSizeWidth;
	target->m_bControlChildSizeHeight = m_bControlChildSizeHeight;
	target->m_bForceExpandWidth = m_bForceExpandWidth;
	target->m_bForceExpandHeight = m_bForceExpandHeight;
}

_bool CLayoutGroup::BuildLayoutContext(LayoutContext& outContext) const
{
	if (!m_pGameObject)
		return false;

	CRectTransform* parentRect = m_pGameObject->GetComponent<CRectTransform>();
	CTransform* transform = m_pGameObject->GetTransform();
	if (!parentRect || !transform)
		return false;

	outContext = {};
	outContext.parentRect = parentRect;
	outContext.parentWidth = max(0.f, parentRect->Get_Width());
	outContext.parentHeight = max(0.f, parentRect->Get_Height());
	outContext.innerWidth = max(0.f, outContext.parentWidth - m_tPadding.left - m_tPadding.right);
	outContext.innerHeight = max(0.f, outContext.parentHeight - m_tPadding.top - m_tPadding.bottom);

	for (CTransform* childTransform : transform->Get_ChldList())
	{
		if (!childTransform)
			continue;

		CGameObject* childObject = childTransform->Get_GameObject();
		if (!childObject || !childObject->IsRecursiveActive())
			continue;

		CRectTransform* childRect = dynamic_cast<CRectTransform*>(childTransform);
		if (!childRect)
			childRect = childObject->GetComponent<CRectTransform>();

		if (!childRect)
			continue;

		outContext.children.push_back(childRect);
	}

	return true;
}

_float CLayoutGroup::GetHorizontalAlignmentFactor() const
{
	switch (m_eChildAlignment)
	{
	case ChildAlignment::UpperCenter:
	case ChildAlignment::MiddleCenter:
	case ChildAlignment::LowerCenter:
		return 0.5f;

	case ChildAlignment::UpperRight:
	case ChildAlignment::MiddleRight:
	case ChildAlignment::LowerRight:
		return 1.f;

	case ChildAlignment::UpperLeft:
	case ChildAlignment::MiddleLeft:
	case ChildAlignment::LowerLeft:
	default:
		return 0.f;
	}
}

_float CLayoutGroup::GetVerticalAlignmentFactor() const
{
	switch (m_eChildAlignment)
	{
	case ChildAlignment::MiddleLeft:
	case ChildAlignment::MiddleCenter:
	case ChildAlignment::MiddleRight:
		return 0.5f;

	case ChildAlignment::UpperLeft:
	case ChildAlignment::UpperCenter:
	case ChildAlignment::UpperRight:
		return 1.f;

	case ChildAlignment::LowerLeft:
	case ChildAlignment::LowerCenter:
	case ChildAlignment::LowerRight:
	default:
		return 0.f;
	}
}

void CLayoutGroup::SetChildLayout(CRectTransform* child, const _float x, const _float y, const _float width, const _float height, const _bool setWidth, const _bool setHeight) const
{
	if (!child)
		return;

	const _float anchorX = GetHorizontalAlignmentFactor();
	const _float anchorY = GetVerticalAlignmentFactor();
	const _float parentWidth = (m_pGameObject && m_pGameObject->GetComponent<CRectTransform>())
		? max(0.f, m_pGameObject->GetComponent<CRectTransform>()->Get_Width())
		: 0.f;
	const _float parentHeight = (m_pGameObject && m_pGameObject->GetComponent<CRectTransform>())
		? max(0.f, m_pGameObject->GetComponent<CRectTransform>()->Get_Height())
		: 0.f;

	child->Set_AnchorsMin(anchorX, anchorY);
	child->Set_AnchorsMax(anchorX, anchorY);

	if (setWidth && setHeight)
		child->Set_WidthHeight(width, height);
	else if (setWidth)
		child->Set_Width(width);
	else if (setHeight)
		child->Set_Height(height);

	const vector2 pivot = child->Get_Pivot();
	const _float finalWidth = setWidth ? width : child->Get_Width();
	const _float finalHeight = setHeight ? height : child->Get_Height();

	child->Set_AnchoredPosition
	(
		x + finalWidth * pivot.x - parentWidth * anchorX,
		y + finalHeight * pivot.y - parentHeight * anchorY
	);
}

void CLayoutGroup::ExecuteLayout()
{
	if (m_bIsApplyingLayout)
		return;

	LayoutContext context = {};
	if (!BuildLayoutContext(context))
		return;

	m_bIsApplyingLayout = true;
	ApplyLayout(context);
	m_bIsApplyingLayout = false;
}
