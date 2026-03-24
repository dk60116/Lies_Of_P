#include "epch.h"
#include "VerticalLayoutGroup.h"

CVerticalLayoutGroup::CVerticalLayoutGroup()
{
	m_strName = L"Vertical Layout Group";
}

CVerticalLayoutGroup::~CVerticalLayoutGroup()
{
}

CVerticalLayoutGroup* CVerticalLayoutGroup::Create()
{
	return new CVerticalLayoutGroup();
}

CComponent* CVerticalLayoutGroup::Clone() const
{
	CVerticalLayoutGroup* clone = new CVerticalLayoutGroup();
	CopyLayoutSettingsTo(clone);
	return clone;
}

void CVerticalLayoutGroup::ApplyLayout(const LayoutContext& context)
{
	if (!context.parentRect || context.children.empty())
		return;

	vector<_float> widths;
	vector<_float> heights;
	widths.reserve(context.children.size());
	heights.reserve(context.children.size());

	_float preferredTotalHeight = 0.f;
	for (CRectTransform* child : context.children)
	{
		if (!child)
			continue;

		const _float childWidth = max(0.f, child->Get_Width());
		const _float childHeight = max(0.f, child->Get_Height());
		widths.push_back(childWidth);
		heights.push_back(childHeight);
		preferredTotalHeight += childHeight;
	}

	const size_t childCount = heights.size();
	if (childCount == 0)
		return;

	const _float spacingTotal = m_fSpacing * static_cast<_float>(max<size_t>(0u, childCount - 1u));
	const _float availableHeight = max(0.f, context.innerHeight - spacingTotal);

	if (m_bControlChildSizeHeight)
	{
		if (m_bForceExpandHeight || preferredTotalHeight <= 1e-4f)
		{
			const _float uniformHeight = (childCount > 0) ? (availableHeight / static_cast<_float>(childCount)) : 0.f;
			for (_float& height : heights)
				height = uniformHeight;
		}
		else
		{
			const _float heightScale = availableHeight / preferredTotalHeight;
			for (_float& height : heights)
				height *= heightScale;
		}
	}
	else if (m_bForceExpandHeight && context.innerHeight > preferredTotalHeight + spacingTotal)
	{
		const _float extraHeightPerChild = (context.innerHeight - preferredTotalHeight - spacingTotal) / static_cast<_float>(childCount);
		for (_float& height : heights)
			height += extraHeightPerChild;
	}

	_float columnHeight = spacingTotal;
	for (const _float height : heights)
		columnHeight += height;

	const _float remainingHeight = context.innerHeight - columnHeight;
	const _float baseY = m_tPadding.bottom + remainingHeight * GetVerticalAlignmentFactor();
	_float cursorTop = baseY + columnHeight;

	for (size_t i = 0; i < context.children.size(); ++i)
	{
		CRectTransform* child = context.children[i];
		if (!child)
			continue;

		_float childWidth = widths[i];
		_float childHeight = heights[i];

		if (m_bControlChildSizeWidth || m_bForceExpandWidth)
			childWidth = context.innerWidth;

		const _float remainingWidth = context.innerWidth - childWidth;
		const _float x = m_tPadding.left + remainingWidth * GetHorizontalAlignmentFactor();

		cursorTop -= childHeight;

		SetChildLayout
		(
			child,
			x,
			cursorTop,
			childWidth,
			childHeight,
			m_bControlChildSizeWidth,
			m_bControlChildSizeHeight
		);

		cursorTop -= m_fSpacing;
	}
}
