#include "epch.h"
#include "HorizontalLayoutGroup.h"

CHorizontalLayoutGroup::CHorizontalLayoutGroup()
{
	m_strName = L"Horizontal Layout Group";
}

CHorizontalLayoutGroup::~CHorizontalLayoutGroup()
{
}

CHorizontalLayoutGroup* CHorizontalLayoutGroup::Create()
{
	return new CHorizontalLayoutGroup();
}

CComponent* CHorizontalLayoutGroup::Clone() const
{
	CHorizontalLayoutGroup* clone = new CHorizontalLayoutGroup();
	CopyLayoutSettingsTo(clone);
	return clone;
}

void CHorizontalLayoutGroup::ApplyLayout(const LayoutContext& context)
{
	if (!context.parentRect || context.children.empty())
		return;

	vector<_float> widths;
	vector<_float> heights;
	widths.reserve(context.children.size());
	heights.reserve(context.children.size());

	_float preferredTotalWidth = 0.f;
	for (CRectTransform* child : context.children)
	{
		if (!child)
			continue;

		const _float childWidth = max(0.f, child->Get_Width());
		const _float childHeight = max(0.f, child->Get_Height());
		widths.push_back(childWidth);
		heights.push_back(childHeight);
		preferredTotalWidth += childWidth;
	}

	const size_t childCount = widths.size();
	if (childCount == 0)
		return;

	const _float spacingTotal = m_fSpacing * static_cast<_float>(max<size_t>(0u, childCount - 1u));
	const _float availableWidth = max(0.f, context.innerWidth - spacingTotal);

	if (m_bControlChildSizeWidth)
	{
		if (m_bForceExpandWidth || preferredTotalWidth <= 1e-4f)
		{
			const _float uniformWidth = (childCount > 0) ? (availableWidth / static_cast<_float>(childCount)) : 0.f;
			for (_float& width : widths)
				width = uniformWidth;
		}
		else
		{
			const _float widthScale = availableWidth / preferredTotalWidth;
			for (_float& width : widths)
				width *= widthScale;
		}
	}
	else if (m_bForceExpandWidth && context.innerWidth > preferredTotalWidth + spacingTotal)
	{
		const _float extraWidthPerChild = (context.innerWidth - preferredTotalWidth - spacingTotal) / static_cast<_float>(childCount);
		for (_float& width : widths)
			width += extraWidthPerChild;
	}

	_float rowWidth = spacingTotal;
	for (const _float width : widths)
		rowWidth += width;

	const _float remainingWidth = context.innerWidth - rowWidth;
	const _float startX = m_tPadding.left + remainingWidth * GetHorizontalAlignmentFactor();

	_float cursorX = startX;
	for (size_t i = 0; i < context.children.size(); ++i)
	{
		CRectTransform* child = context.children[i];
		if (!child)
			continue;

		_float childWidth = widths[i];
		_float childHeight = heights[i];

		if (m_bControlChildSizeHeight || m_bForceExpandHeight)
			childHeight = context.innerHeight;

		const _float remainingHeight = context.innerHeight - childHeight;
		const _float y = m_tPadding.bottom + remainingHeight * GetVerticalAlignmentFactor();

		SetChildLayout
		(
			child,
			cursorX,
			y,
			childWidth,
			childHeight,
			m_bControlChildSizeWidth,
			m_bControlChildSizeHeight
		);

		cursorX += childWidth + m_fSpacing;
	}
}
