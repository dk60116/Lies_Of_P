#include "epch.h"
#include "Light.h"

namespace
{
constexpr _float kShadowCascadeMinRange = 0.001f;

	vector3 NormalizeOrDefault(const vector3& value, const vector3& fallback)
	{
		const _float lenSq = value.x * value.x + value.y * value.y + value.z * value.z;
		if (lenSq <= 1e-6f)
			return fallback;

		const _float invLen = 1.0f / sqrtf(lenSq);
		return vector3(value.x * invLen, value.y * invLen, value.z * invLen);
	}

	void StoreShadowMatrices(const _matrix& view, const _matrix& proj, CLight::ShadowMatrices& outMatrices)
	{
		XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&outMatrices.view), view);
		XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&outMatrices.proj), proj);
	}

	void BuildLegacyDirectionalShadowCascade(
		const vector3& camPos,
		const vector3& camForward,
		const vector3& lightDir,
		const _float shadowDistance,
		const _float splitDepth,
		CLight::ShadowCascade& outCascade)
	{
		const vector3 center = camPos + camForward * (shadowDistance * 0.5f);
		const vector3 lightPos = center - lightDir * shadowDistance;

		const _vector eye = XMVectorSet(lightPos.x, lightPos.y, lightPos.z, 1.f);
		const _vector at = XMVectorSet(center.x, center.y, center.z, 1.f);

		const _vector worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
		const _vector ldir = XMVector3Normalize(XMVectorSet(lightDir.x, lightDir.y, lightDir.z, 0.f));

		const _float upDot = fabsf(XMVectorGetX(XMVector3Dot(ldir, worldUp)));
		const _vector up = (upDot > 0.99f) ? XMVectorSet(0.f, 0.f, 1.f, 0.f) : worldUp;

		const _matrix view = XMMatrixLookAtLH(eye, at, up);

		const _float half = shadowDistance * 0.5f;
		const _float nearZ = 0.0f;
		const _float farZ = shadowDistance * 2.0f;
		const _matrix proj = XMMatrixOrthographicOffCenterLH(-half, half, -half, half, nearZ, farZ);

		StoreShadowMatrices(view, proj, outCascade.matrices);
		outCascade.splitDepth = splitDepth;
		outCascade.lightSpaceMin = _float3(-half, -half, nearZ);
		outCascade.lightSpaceMax = _float3(half, half, farZ);
	}

	void BuildPerspectiveCascade(
		const vector3& camPos,
		const CTransform::DIRECTIONS& camDirections,
		const _float fovRadians,
		const _float aspect,
		const _float cascadeNear,
		const _float cascadeFar,
		const vector3& lightDir,
		const _float casterExtrusionDistance,
		const _uint shadowMapSize,
		CLight::ShadowCascade& outCascade)
	{
		const vector3 forward = NormalizeOrDefault(camDirections.forward, vector3(0.f, 0.f, 1.f));
		const vector3 right = NormalizeOrDefault(camDirections.right, vector3(1.f, 0.f, 0.f));
		const vector3 up = NormalizeOrDefault(camDirections.up, vector3(0.f, 1.f, 0.f));

		const _float tanHalfFov = tanf(fovRadians * 0.5f);
		const _float nearHalfH = tanHalfFov * cascadeNear;
		const _float nearHalfW = nearHalfH * aspect;
		const _float farHalfH = tanHalfFov * cascadeFar;
		const _float farHalfW = farHalfH * aspect;

		const vector3 nearCenter = camPos + forward * cascadeNear;
		const vector3 farCenter = camPos + forward * cascadeFar;

		array<vector3, 8> corners =
		{
			nearCenter + up * nearHalfH - right * nearHalfW,
			nearCenter + up * nearHalfH + right * nearHalfW,
			nearCenter - up * nearHalfH - right * nearHalfW,
			nearCenter - up * nearHalfH + right * nearHalfW,
			farCenter + up * farHalfH - right * farHalfW,
			farCenter + up * farHalfH + right * farHalfW,
			farCenter - up * farHalfH - right * farHalfW,
			farCenter - up * farHalfH + right * farHalfW
		};

		array<vector3, 8> casterCorners = {};
		for (_uint i = 0u; i < corners.size(); ++i)
			casterCorners[i] = corners[i] - lightDir * max(casterExtrusionDistance, 0.f);

		vector3 frustumCenter = vector3::zero();
		for (const vector3& corner : corners)
			frustumCenter += corner;
		frustumCenter /= static_cast<_float>(corners.size());

		_float radius = 0.f;
		for (const vector3& corner : corners)
		{
			const vector3 delta = corner - frustumCenter;
			radius = max(radius, sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z));
		}

		const vector3 lightPos = frustumCenter - lightDir * (radius + 100.f);
		const _vector eye = XMVectorSet(lightPos.x, lightPos.y, lightPos.z, 1.f);
		const _vector at = XMVectorSet(frustumCenter.x, frustumCenter.y, frustumCenter.z, 1.f);
		const _vector worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
		const _vector lightDirV = XMVector3Normalize(XMVectorSet(lightDir.x, lightDir.y, lightDir.z, 0.f));
		const _float upDot = fabsf(XMVectorGetX(XMVector3Dot(lightDirV, worldUp)));
		const _vector lightUp = (upDot > 0.99f) ? XMVectorSet(0.f, 0.f, 1.f, 0.f) : worldUp;

		const _matrix view = XMMatrixLookAtLH(eye, at, lightUp);

		_float minX = FLT_MAX;
		_float minY = FLT_MAX;
		_float minZ = FLT_MAX;
		_float maxX = -FLT_MAX;
		_float maxY = -FLT_MAX;
		_float maxZ = -FLT_MAX;

		for (const vector3& corner : corners)
		{
			const _vector cornerLS = XMVector3TransformCoord(XMVectorSet(corner.x, corner.y, corner.z, 1.f), view);
			minX = min(minX, XMVectorGetX(cornerLS));
			minY = min(minY, XMVectorGetY(cornerLS));
			minZ = min(minZ, XMVectorGetZ(cornerLS));
			maxX = max(maxX, XMVectorGetX(cornerLS));
			maxY = max(maxY, XMVectorGetY(cornerLS));
			maxZ = max(maxZ, XMVectorGetZ(cornerLS));
		}

		// Include potential casters outside the receiver frustum so long shadows
		// do not get clipped as the sun angle changes.
		for (const vector3& corner : casterCorners)
		{
			const _vector cornerLS = XMVector3TransformCoord(XMVectorSet(corner.x, corner.y, corner.z, 1.f), view);
			minX = min(minX, XMVectorGetX(cornerLS));
			minY = min(minY, XMVectorGetY(cornerLS));
			minZ = min(minZ, XMVectorGetZ(cornerLS));
			maxX = max(maxX, XMVectorGetX(cornerLS));
			maxY = max(maxY, XMVectorGetY(cornerLS));
			maxZ = max(maxZ, XMVectorGetZ(cornerLS));
		}

		const _float orthoWidth = max(maxX - minX, 1.0f);
		const _float orthoHeight = max(maxY - minY, 1.0f);
		const _float safeShadowMapSize = static_cast<_float>(max<_uint>(shadowMapSize, 1u));
		const _float texelSizeX = orthoWidth / safeShadowMapSize;
		const _float texelSizeY = orthoHeight / safeShadowMapSize;

		_float centerX = (minX + maxX) * 0.5f;
		_float centerY = (minY + maxY) * 0.5f;
		centerX = roundf(centerX / max(texelSizeX, 1e-6f)) * texelSizeX;
		centerY = roundf(centerY / max(texelSizeY, 1e-6f)) * texelSizeY;

		minX = centerX - orthoWidth * 0.5f;
		maxX = centerX + orthoWidth * 0.5f;
		minY = centerY - orthoHeight * 0.5f;
		maxY = centerY + orthoHeight * 0.5f;

		const _float zPadding = max(25.f, radius * 0.5f);
		minZ -= zPadding;
		maxZ += zPadding;
		if (maxZ <= minZ)
			maxZ = minZ + 1.f;

		const _matrix proj = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);

		StoreShadowMatrices(view, proj, outCascade.matrices);
		outCascade.splitDepth = cascadeFar;
		outCascade.lightSpaceMin = _float3(minX, minY, minZ);
		outCascade.lightSpaceMax = _float3(maxX, maxY, maxZ);
	}
}

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
	m_strName = L"Light";
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
	clone->m_bCastShadow = this->m_bCastShadow;

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
	m_fIntensity = max(0.f, _value);
}

const _float CLight::Get_Range() const
{
	return m_fRange;
}

void CLight::Set_Range(const _float _value)
{
	m_fRange = max(0.f, _value);
}

const _float CLight::Get_SpotAngle() const
{
	return m_fSpotAngle;
}

void CLight::Set_SpotAngle(const _float _value)
{
	m_fSpotAngle = clamp(_value, 1.f, 179.f);
}

const _float CLight::Get_Attenuation() const
{
	return m_fAttenuation;
}

void CLight::Set_Attenuation(const _float _value)
{
	m_fAttenuation = max(0.f, _value);
}

const ColorValue& CLight::Get_DiffuseColor() const
{
	return m_vDiffuseColor;
}

void CLight::Set_Color(const ColorValue _color)
{
	m_vDiffuseColor = _color;
}

const ColorValue& CLight::Get_SpecularColor() const
{
	return m_vSpecularColor;
}

void CLight::Set_SpecularColor(const ColorValue _color)
{
	m_vSpecularColor = _color;
}

const _float4x4 CLight::To_LightInfo()
{
	_float4x4 result = {};

	const vector3 pos = GetTransform()->Get_Position();
	vector3 dir = GetTransform()->Get_Directions().forward;
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

void CLight::BuildDirectionalShadows(CCamera* _cam, _float _shadowDistance, DirectionalShadowData& _outShadowData)
{
	_outShadowData = {};

	if (!_cam)
		return;

	const vector3 camPos = _cam->GetTransform()->Get_Position();
	const CTransform::DIRECTIONS& camDirections = _cam->GetTransform()->Get_Directions();
	const vector3 camForward = NormalizeOrDefault(camDirections.forward, vector3(0.f, 0.f, 1.f));
	const vector3 lightDir = NormalizeOrDefault(GetTransform()->Get_Directions().forward, vector3(0.f, 0.f, 1.f));

	const _float cameraNear = max(_cam->GetNear(), 0.01f);
	const _float receiverMaxDistance = max(cameraNear + kShadowCascadeMinRange, min(_cam->GetFar(), max(_shadowDistance, cameraNear + kShadowCascadeMinRange)));
	const _float lightVertical = fabsf(lightDir.y);
	const _float lightAngleStretch = min(1.0f / max(lightVertical, 0.15f), 6.0f);
	const _float casterExtrusionDistance = receiverMaxDistance * lightAngleStretch;
	const _uint shadowMapSize = max(CSceneManager::GetInstance().Get_LightSetting().shadowMapSize, 1u);

	_outShadowData.cascadeCount = kMaxShadowCascades;

	if (_cam->GetViewMode() != CCamera::ViewMode::Perspective)
	{
		for (_uint cascadeIndex = 0u; cascadeIndex < kMaxShadowCascades; ++cascadeIndex)
		{
			const _float t = static_cast<_float>(cascadeIndex + 1u) / static_cast<_float>(kMaxShadowCascades);
			const _float splitDepth = cameraNear + (receiverMaxDistance - cameraNear) * t;
			BuildLegacyDirectionalShadowCascade(camPos, camForward, lightDir, receiverMaxDistance, splitDepth, _outShadowData.cascades[cascadeIndex]);
		}

		return;
	}

	const _float farToNearRatio = max(receiverMaxDistance / max(cameraNear, 0.0001f), 1.0f);
	const _float clipRange = receiverMaxDistance - cameraNear;
	const _float fovRadians = XMConvertToRadians(max(_cam->GetFieldOfView(), 1.f));
	const _float aspect = max(_cam->GetAspect(), 0.001f);
	const CScene* const currentScene = CSceneManager::GetInstance().Get_CrtScene();
	_float shadowSplitLambda = 0.6f;
	if (currentScene)
		shadowSplitLambda = max(0.f, min(currentScene->Get_EnviromentSetting().directionalShadowSplitLambda, 1.f));

	_float prevSplitDepth = cameraNear;

	for (_uint cascadeIndex = 0u; cascadeIndex < kMaxShadowCascades; ++cascadeIndex)
	{
		const _float p = static_cast<_float>(cascadeIndex + 1u) / static_cast<_float>(kMaxShadowCascades);
		const _float logSplit = cameraNear * powf(farToNearRatio, p);
		const _float uniformSplit = cameraNear + clipRange * p;
		_float cascadeSplitDepth = uniformSplit + (logSplit - uniformSplit) * shadowSplitLambda;

		if (cascadeIndex + 1u == kMaxShadowCascades)
			cascadeSplitDepth = receiverMaxDistance;

		cascadeSplitDepth = max(cascadeSplitDepth, prevSplitDepth + kShadowCascadeMinRange);
		cascadeSplitDepth = min(cascadeSplitDepth, receiverMaxDistance);

		BuildPerspectiveCascade(
			camPos,
			camDirections,
			fovRadians,
			aspect,
			prevSplitDepth,
			cascadeSplitDepth,
			lightDir,
			casterExtrusionDistance,
			shadowMapSize,
			_outShadowData.cascades[cascadeIndex]);

		prevSplitDepth = cascadeSplitDepth;
	}
}


