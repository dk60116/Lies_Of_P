#pragma once

#include "Component.h"
#include <array>

NS_BEGIN(Engine)

class ENGINE_DLL CLight final : public CComponent
{
	friend class CGameObject;

public:
	struct ShadowMatrices
	{
		_float4x4 view;
		_float4x4 proj;
	};

	struct ShadowCascade
	{
		ShadowMatrices matrices = {};
		_float splitDepth = 0.f;
		_float3 lightSpaceMin = {};
		_float padding0 = 0.f;
		_float3 lightSpaceMax = {};
		_float padding1 = 0.f;
	};

	struct DirectionalShadowData
	{
		array<ShadowCascade, kMaxShadowCascades> cascades = {};
		_uint cascadeCount = 0u;
		_float3 padding = {};
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

	void BuildDirectionalShadows(class CCamera* _cam, _float _shadowDistance, DirectionalShadowData& _outShadowData);

private:
	Type m_eType;

	_float m_fIntensity, m_fRange, m_fSpotAngle, m_fAttenuation;
	ColorValue m_vDiffuseColor, m_vSpecularColor;

private:
	bool m_bCastShadow;
};

NS_END


