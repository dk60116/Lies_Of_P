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

	enum class Radial90_FillOrigin
	{
		BottomLeft,
		TopLeft,
		TopRight,
		BottomRight
	};

	enum class Radial180_FillOrigin
	{
		Bottom,
		Left,
		Top,
		Right
	};

	enum class Radial360_FillOrigin
	{
		Bottom,
		Right,
		Top,
		Left
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
	_int Get_FillOrigin() const;
	void Set_FillOrigin(_int _fillOrigin);
	_bool Get_FillClockwise() const;
	void Set_FillClockwise(_bool _fillClockwise);

	const _float GetFillAmount() const;
	void SetFillAmount(_float _fill);

	void Bind_UIMaterial();

	const _int GetGroupID() const;
	void SetGroupID(const _int _id);
	
public:
	void SetTexture(CTexture* _texture);
	CTexture* GetTexture() const;

private:
	class CTexture* m_pTexture;
	_float m_fFillAmount;
	_int m_iFillOrigin;
	_bool m_bFillClockwise;

private:
	FillMethod m_eFillMethod;

	ID3D11Buffer* m_pImageBuffer;

	_int m_iBatchGroupId;
};

NS_END

