#pragma once

#include "UI.h"

NS_BEGIN(Engine)

class ENGINE_DLL CText final : public CUI
{
	friend class CGameObject;

private:
	CText();
	~CText();

private:
	static CText* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Render_Editor() override;
	void RenderText();
	void OnDestroy() override;

public:
	void Set_Font(class CFont* _font);
	void Set_FontSize(const _float _size);
	void Set_Text(wstring _text);
	void Set_Text(string _text);

private:
	CFont* m_pFont;
	wstring m_strText;
	_float m_fFontSize;
};

NS_END

