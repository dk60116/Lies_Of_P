#include "epch.h"
#include "Light.h"

CLight::CLight()
	: m_eType(Type::Directional)
	, m_fIntensity(1.f)
	, m_fRange(10.f)
	, m_fSpotAngle(45.f)
	, m_fAttenuation(1.f)
	, m_vDiffuseColor(ColorValue::white())
	, m_vSpecularColor(ColorValue::white())
	, m_bCastShadow(true)
{
}

CLight::~CLight()
{
}

CLight* CLight::Create()
{
	return new CLight();
}

CComponent* CLight::Clone() const
{
	CLight* clone = new CLight;

	clone->m_eType = this->m_eType;
	clone->m_fIntensity = this->m_fIntensity;
	clone->m_fRange = this->m_fRange;
	clone->m_fSpotAngle = this->m_fSpotAngle;
	clone->m_fAttenuation = this->m_fAttenuation;
	clone->m_vDiffuseColor = this->m_vDiffuseColor;
	clone->m_vSpecularColor = this->m_vSpecularColor;

	return clone;
}

HRESULT CLight::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	return S_OK;
}

void CLight::Update()
{
}

void CLight::Render_Editor()
{
}

void CLight::Render()
{
}

void CLight::OnDestroy()
{
}

const CLight::Type CLight::Get_Type() const
{
	return m_eType;
}

void CLight::Set_Type(const Type _type)
{
	m_eType = _type;
}

const _float CLight::Get_Intensity() const
{
	return m_fIntensity;
}

void CLight::Set_Intensity(const _float _value)
{
	m_fIntensity = _value;
}

void CLight::Set_Range(const _float _value)
{
	m_fRange = _value;
}

void CLight::Set_Color(const ColorValue _color)
{
	m_vDiffuseColor = _color;
}

const _float4x4 CLight::To_LightInfo()
{
	_float4x4 result = {};

	const vector3 pos = Get_Transform()->Get_Position();
	vector3 dir = Get_Transform()->Get_Directions().forward;
	dir = dir.normalized();
	const vector3 color = m_vDiffuseColor.f3Color();

	result._11 = pos.x;
	result._12 = pos.y;
	result._13 = pos.z;
	result._14 = m_fRange;

	result._21 = dir.x;
	result._22 = dir.y;
	result._23 = dir.z;
	result._24 = m_fIntensity;

	result._31 = color.x;
	result._32 = color.y;
	result._33 = color.z;
	result._34 = CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().ambient;

	result._41 = static_cast<_float>(m_eType);
	result._42 = m_fAttenuation;
	result._43 = (m_pGameObject->IsActive() && m_bEnable) ? 1.f : 0.f;
	result._44 = cosf(XMConvertToRadians(m_fSpotAngle * 0.5f));

	return result;
}

const bool CLight::IsCastShadow() const
{
	return m_bCastShadow;
}

void CLight::SetCastShadow(_bool _value)
{
	m_bCastShadow = _value;
}

void CLight::BuildDirectionalShadow(CCamera* _cam, _float _shadowDistance, ShadowMatrices& _outShadowMatix)
{
	if (!_cam)
		return;

	vector3 camPos = _cam->Get_Transform()->Get_Position();
	vector3 camFwd = _cam->Get_Transform()->Get_Directions().forward;
	camFwd = camFwd.normalized();

	vector3 center = camPos + camFwd * (_shadowDistance * 0.5f);

	vector3 lightDir = Get_Transform()->Get_Directions().forward;
	lightDir = lightDir.normalized();

	vector3 lightPos = center - lightDir * _shadowDistance;

	_vector eye = XMVectorSet(lightPos.x, lightPos.y, lightPos.z, 1.f);
	_vector at = XMVectorSet(center.x, center.y, center.z, 1.f);

	_vector worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	_vector ldir = XMVector3Normalize(XMVectorSet(lightDir.x, lightDir.y, lightDir.z, 0.f));

	_float upDot = fabsf(XMVectorGetX(XMVector3Dot(ldir, worldUp)));
	_vector up = (upDot > 0.99f) ? XMVectorSet(0.f, 0.f, 1.f, 0.f) : worldUp;

	_matrix V = XMMatrixLookAtLH(eye, at, up);

	_float half = _shadowDistance * 0.5f;
	_float nearZ = 0.0f;
	_float farZ = _shadowDistance * 2.0f;

	const _uint shadowMapSize = CSceneManager::GetInstance().Get_LightSetting().shadowMapSize;
	const _float texelSize = (shadowMapSize > 0) ? (_shadowDistance / (_float)shadowMapSize) : 0.f;

	if (texelSize > 0.f)
	{
		_vector centerWS = XMVectorSet(center.x, center.y, center.z, 1.f);
		_vector centerLS = XMVector3TransformCoord(centerWS, V);

		_float snappedX = floorf(XMVectorGetX(centerLS) / texelSize) * texelSize;
		_float snappedY = floorf(XMVectorGetY(centerLS) / texelSize) * texelSize;

		_vector snapOffsetLS = XMVectorSet(snappedX - XMVectorGetX(centerLS), snappedY - XMVectorGetY(centerLS), 0.f, 0.f);
		_matrix invV = XMMatrixInverse(nullptr, V);
		_vector snapOffsetWS = XMVector3TransformNormal(snapOffsetLS, invV);

		eye = XMVectorAdd(eye, snapOffsetWS);
		at = XMVectorAdd(at, snapOffsetWS);
		V = XMMatrixLookAtLH(eye, at, up);
	}

	_matrix P = XMMatrixOrthographicOffCenterLH(-half, half, -half, half, nearZ, farZ);

	XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&_outShadowMatix.view), V);
	XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&_outShadowMatix.proj), P);
}
