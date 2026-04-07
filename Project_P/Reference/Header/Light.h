#pragma once

#include "Component.h"

NS_BEGIN(Engine)

class ENGINE_DLL CLight final : public CComponent
{
	friend class CGameObject;

public:
    struct ShadowCascadeMatrix
    {
        _float4x4 view = {};
        _float4x4 proj = {};
        _float splitDepth = 0.f;
        _float shadowPadding0 = 0.f;
        _float4 atlasScaleOffset = { 1.f, 1.f, 0.f, 0.f };
        _float4 lightSpaceBounds = { 0.f, 0.f, 0.f, 0.f };
        _float4 lightSpaceDepthRange = { 0.f, 0.f, 0.f, 0.f };
    };

    struct ShadowMatrices
    {
        _float4x4 view = {};
        _float4x4 proj = {};
        ShadowCascadeMatrix cascades[kMaxShadowCascades] = {};
        _uint cascadeCount = 1u;
        _float shadowDistance = 0.f;
        _float splitLambda = 0.f;
        _float shadowPadding0 = 0.f;
    };

public:
	enum class Type : _uint { Directional = 0, point = 1, spot = 2 };

protected:
	explicit CLight();
	~CLight();

private:
	static CLight* Create();
	CComponent* Clone() const override;

public:
	HRESULT Initialize() override;
	void Update() override;
	void Render_Editor() override;
	void Render() override;

	void OnDestroy() override;

public:
	const Type Get_Type() const;
	void Set_Type(const Type _type);
	const _float Get_Intensity() const;
	void Set_Intensity(const _float _value);
	const _float Get_Range() const;
	const _float Get_SpotAngle() const;
	void Set_SpotAngle(const _float _value);
	const _float Get_Attenuation() const;
	void Set_Attenuation(const _float _value);
	const ColorValue& Get_DiffuseColor() const;
	void Set_Color(const ColorValue _color);
	const ColorValue& Get_SpecularColor() const;
	void Set_SpecularColor(const ColorValue _color);
	void Set_Range(const _float _value);

	const _float4x4 To_LightInfo();

public:
	const bool IsCastShadow() const;
	void SetCastShadow(_bool _value);

	void BuildDirectionalShadow(class CCamera* _cam, _float _shadowDistance, ShadowMatrices& _outShadowMatix);

private:
	Type m_eType;

	_float m_fIntensity, m_fRange, m_fSpotAngle, m_fAttenuation;
	ColorValue m_vDiffuseColor, m_vSpecularColor;

private:
	bool m_bCastShadow;
};

NS_END


