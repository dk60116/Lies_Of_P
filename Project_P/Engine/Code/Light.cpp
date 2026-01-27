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
	const vector3 dir = Get_Transform()->Get_Directions().forward;
	const vector3 color = m_vDiffuseColor.f3Color();

	result._11 = pos.x;
	result._12 = pos.y;
	result._13 = pos.z;
	result._14 = m_fRange;

	result._21 = dir.x;
	result._22 = dir.y;
	result._23 = dir.z;
	result._24 = cosf(XMConvertToRadians(m_fSpotAngle * 0.5f));

	result._31 = color.x;
	result._32 = color.y;
	result._33 = color.z;
	result._34 = CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().ambient;

	result._41 = static_cast<float>(m_eType);
	result._42 = m_fAttenuation;
	result._43 = (m_pGameObject->IsActive() && m_bEnable) ? 1.f : 0.f;
	result._44 = 0.f;

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

	// 1) 카메라 중심점(간이)
	vector3 camPos = _cam->Get_Transform()->Get_Position();
	vector3 camFwd = _cam->Get_Transform()->Get_Directions().forward;
	camFwd = camFwd.normalized(); // 엔진 함수에 맞게 교체

	vector3 center = camPos + camFwd * (_shadowDistance * 0.5f);

	// 2) 라이트 방향
	vector3 lightDir = Get_Transform()->Get_Directions().forward;
	lightDir = lightDir.normalized(); // 필수 권장

	vector3 lightPos = center - lightDir * _shadowDistance;

	_vector eye = XMVectorSet(lightPos.x, lightPos.y, lightPos.z, 1.f);
	_vector at = XMVectorSet(center.x, center.y, center.z, 1.f);

	// 3) Up 특이점 방지
	_vector worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	_vector ldir = XMVector3Normalize(XMVectorSet(lightDir.x, lightDir.y, lightDir.z, 0.f));

	_float upDot = fabsf(XMVectorGetX(XMVector3Dot(ldir, worldUp)));
	_vector up = (upDot > 0.99f) ? XMVectorSet(0.f, 0.f, 1.f, 0.f) : worldUp;

	_matrix V = XMMatrixLookAtLH(eye, at, up);

	// 4) Ortho (간이)
	_float half = _shadowDistance * 0.5f;
	_float nearZ = 0.0f;
	_float farZ = _shadowDistance * 2.0f;

	_matrix P = XMMatrixOrthographicOffCenterLH(-half, half, -half, half, nearZ, farZ);

	// 5) Store (reinterpret_cast 지양)
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&_outShadowMatix.view), V);
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&_outShadowMatix.proj), P);
}
