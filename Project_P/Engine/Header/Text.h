#pragma once

#include "UI.h"

NS_BEGIN(Engine)

class ENGINE_DLL CText final : public CUI
{
	friend class CGameObject;

public:
	enum class TextAligmentHorizontal { Left, Center, Right, JustFied, Flush };
	enum class TexAligmentVertical { Top, Middle, Bottom };

private:
	CText();
	~CText();

private:
	static CText* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Render_Editor() override;
	void Render_Gizmo() override;
	void RenderText();
	void RenderText_Editor();
	void OnDestroy() override;

public:
	void SetFont(class CFont* _font);
	void SetFontSize(const _float _size);
	void SetText(const wstring& _text);
	void SetText(const string& _text);
	void ResetText();
	class CFont* GetFont() const;
	const wstring& GetText() const;
	_float GetFontSize() const;
	TextAligmentHorizontal GetAligmentHorizontal() const;
	TexAligmentVertical GetAligmentVertical() const;

	void SetAligmentHorizontal(const TextAligmentHorizontal _type);
	void SetAligmentVertical(const TexAligmentVertical _type);

private:
	CFont* m_pFont;
	wstring m_strText;
	_float m_fFontSize;
	TextAligmentHorizontal m_eAlignmentHorizontal;
	TexAligmentVertical m_eAlignmentVertical;
};

NS_END

