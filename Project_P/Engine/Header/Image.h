#pragma once

#include "UI.h"

NS_BEGIN(Engine)

class ENGINE_DLL CImage final : public CUI
{
	friend class CGameObject;

public:
	enum class FillMethod
	{
		None,
		Horizontal,
		Vertical,
		Radial90,
		Radial180,
		Radial360
	};

	enum class Horizontal_FillOrigin
	{
		Left,
		Right
	};

	enum class Vertical_FillOrigin
	{
		Bottom,
		Top
	};

private:
	CImage();
	~CImage();

private:
	static CImage* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Render_Editor() override;
	void Render() override;
	void OnDestroy() override;

public:
	const FillMethod Get_FillMethod() const;
	void Set_FillMethod(FillMethod _fillMethod);

	const _float GetFillAmount() const;
	void SetFillAmount(_float _fill);

	void Bind_UIMaterial();
	
public:
	void SetTexture(CTexture* _texture);

private:
	class CTexture* m_pTexture;
	_float m_fFillAmount;

private:
	FillMethod m_eFillMethod;

	ID3D11Buffer* m_pImageBuffer;
};

NS_END

