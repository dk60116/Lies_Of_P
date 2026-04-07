#include "epch.h"
#include "Light.h"

namespace
{
	constexpr _uint kDefaultShadowCascadeCount = 4u;
	constexpr _float kDefaultShadowSplitLambda = 0.65f;
	constexpr _float kMinCascadeDepthRange = 0.01f;
	constexpr _float kShadowCascadeEyeDistanceScale = 4.0f;
	constexpr _float kShadowCascadeCasterDepthPaddingScale = 2.0f;
	constexpr _float kMinShadowCascadeCasterDepthPadding = 20.0f;
	constexpr _float kShadowCascadeRadiusQuantizeScale = 16.0f;

	_float ResolveCascadeTileResolution(_float atlasScale)
	{
		const _float atlasResolution = static_cast<_float>(CSceneManager::GetInstance().Get_LightSetting().shadowMapSize);
		if (atlasResolution <= 0.f)
			return 1.f;

		return max(atlasResolution * max(atlasScale, 0.001f), 1.f);
	}

	void ApplyShadowTexelSnapping(const _matrix& lightView, _matrix& lightProj, const _float tileResolution)
	{
		if (tileResolution <= 1.f)
			return;

		const _matrix shadowMatrix = XMMatrixMultiply(lightView, lightProj);
		_vector shadowOrigin = XMVector3TransformCoord(XMVectorZero(), shadowMatrix);
		shadowOrigin = XMVectorScale(shadowOrigin, tileResolution * 0.5f);

		const _vector roundedOrigin = XMVectorRound(shadowOrigin);
		const _vector roundOffset = XMVectorScale(XMVectorSubtract(roundedOrigin, shadowOrigin), 2.0f / tileResolution);
		lightProj.r[3] = XMVectorAdd(lightProj.r[3], XMVectorSet(XMVectorGetX(roundOffset), XMVectorGetY(roundOffset), 0.f, 0.f));
	}

	_float4 BuildCascadeAtlasScaleOffset(const _uint cascadeIndex)
	{
		const _uint column = cascadeIndex % 2u;
		const _uint row = cascadeIndex / 2u;
		return _float4(0.5f, 0.5f, 0.5f * static_cast<_float>(column), 0.5f * static_cast<_float>(row));
	}

	_vector ResolveShadowUpVector(const vector3& lightDir)
	{
		const _vector worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
		const _vector lightDirVector = XMVector3Normalize(XMVectorSet(lightDir.x, lightDir.y, lightDir.z, 0.f));
		const _float upDot = fabsf(XMVectorGetX(XMVector3Dot(lightDirVector, worldUp)));
		return (upDot > 0.99f) ? XMVectorSet(0.f, 0.f, 1.f, 0.f) : worldUp;
	}

	void WriteWorldCorner(const vector3& position, const _uint index, XMFLOAT3(&outCorners)[8])
	{
		if (index >= 8u)
			return;

		outCorners[index] = XMFLOAT3(position.x, position.y, position.z);
	}

	void BuildPerspectiveCascadeCorners(CCamera* camera, const _float splitNear, const _float splitFar, XMFLOAT3(&outCorners)[8])
	{
		if (!camera || !camera->GetTransform())
			return;

		const CTransform::DIRECTIONS& directions = camera->GetTransform()->Get_Directions();
		const vector3 camPos = camera->GetTransform()->Get_Position();
		const vector3 camForward = directions.forward.normalized();
		const vector3 camRight = directions.right.normalized();
		const vector3 camUp = directions.up.normalized();

		const _float aspect = max(camera->GetAspect(), 0.001f);
		const _float fovRad = XMConvertToRadians(camera->GetFieldOfView());
		const _float tanHalfFovY = tanf(fovRad * 0.5f);
		const _float tanHalfFovX = tanHalfFovY * aspect;

		const _float nearWidth = tanHalfFovX * splitNear;
		const _float nearHeight = tanHalfFovY * splitNear;
		const _float farWidth = tanHalfFovX * splitFar;
		const _float farHeight = tanHalfFovY * splitFar;

		const vector3 nearCenter = camPos + camForward * splitNear;
		const vector3 farCenter = camPos + camForward * splitFar;

		WriteWorldCorner(nearCenter - camRight * nearWidth + camUp * nearHeight, 0u, outCorners);
		WriteWorldCorner(nearCenter + camRight * nearWidth + camUp * nearHeight, 1u, outCorners);
		WriteWorldCorner(nearCenter + camRight * nearWidth - camUp * nearHeight, 2u, outCorners);
		WriteWorldCorner(nearCenter - camRight * nearWidth - camUp * nearHeight, 3u, outCorners);
		WriteWorldCorner(farCenter - camRight * farWidth + camUp * farHeight, 4u, outCorners);
		WriteWorldCorner(farCenter + camRight * farWidth + camUp * farHeight, 5u, outCorners);
		WriteWorldCorner(farCenter + camRight * farWidth - camUp * farHeight, 6u, outCorners);
		WriteWorldCorner(farCenter - camRight * farWidth - camUp * farHeight, 7u, outCorners);
	}

	void BuildOrthographicCascadeCorners(CCamera* camera, const _float splitNear, const _float splitFar, XMFLOAT3(&outCorners)[8])
	{
		if (!camera || !camera->GetTransform())
			return;

		const CTransform::DIRECTIONS& directions = camera->GetTransform()->Get_Directions();
		const vector3 camPos = camera->GetTransform()->Get_Position();
		const vector3 camForward = directions.forward.normalized();
		const vector3 camRight = directions.right.normalized();
		const vector3 camUp = directions.up.normalized();

		const _float halfHeight = max(camera->GetOrthographicSize() * 0.5f, 0.001f);
		const _float halfWidth = max(halfHeight * max(camera->GetAspect(), 0.001f), 0.001f);

		const vector3 nearCenter = camPos + camForward * splitNear;
		const vector3 farCenter = camPos + camForward * splitFar;

		WriteWorldCorner(nearCenter - camRight * halfWidth + camUp * halfHeight, 0u, outCorners);
		WriteWorldCorner(nearCenter + camRight * halfWidth + camUp * halfHeight, 1u, outCorners);
		WriteWorldCorner(nearCenter + camRight * halfWidth - camUp * halfHeight, 2u, outCorners);
		WriteWorldCorner(nearCenter - camRight * halfWidth - camUp * halfHeight, 3u, outCorners);
		WriteWorldCorner(farCenter - camRight * halfWidth + camUp * halfHeight, 4u, outCorners);
		WriteWorldCorner(farCenter + camRight * halfWidth + camUp * halfHeight, 5u, outCorners);
		WriteWorldCorner(farCenter + camRight * halfWidth - camUp * halfHeight, 6u, outCorners);
		WriteWorldCorner(farCenter - camRight * halfWidth - camUp * halfHeight, 7u, outCorners);
	}

	void BuildCascadeShadowMatrix
	(
		const XMFLOAT3(&frustumCorners)[8],
		const vector3& lightDir,
		const _float splitFar,
		const _float4& atlasScaleOffset,
		CLight::ShadowCascadeMatrix& outCascade
	)
	{
		vector3 center = vector3::zero();

		for (const XMFLOAT3& corner : frustumCorners)
			center += vector3(corner.x, corner.y, corner.z);

		center /= 8.f;

		_float radius = 0.f;
		for (const XMFLOAT3& corner : frustumCorners)
		{
			const vector3 cornerVec(corner.x, corner.y, corner.z);
			radius = max(radius, vector3::Distance(center, cornerVec));
		}

		radius = max(radius, 1.f);
		radius = ceilf(radius * kShadowCascadeRadiusQuantizeScale) / kShadowCascadeRadiusQuantizeScale;

		// Keep the light camera further back so tall casters upstream of the
		// receiver slice are less likely to get clipped at the near plane.
		const _float eyeDistance = max(radius * kShadowCascadeEyeDistanceScale, splitFar + radius);
		const vector3 eyePos = center - lightDir * eyeDistance;
		const _vector eye = XMVectorSet(eyePos.x, eyePos.y, eyePos.z, 1.f);
		const _vector at = XMVectorSet(center.x, center.y, center.z, 1.f);
		const _vector up = ResolveShadowUpVector(lightDir);

		const _matrix lightView = XMMatrixLookAtLH(eye, at, up);

		_float minZ = FLT_MAX;
		_float maxZ = -FLT_MAX;

		for (const XMFLOAT3& corner : frustumCorners)
		{
			const _vector worldCorner = XMLoadFloat3(&corner);
			const _vector lightSpaceCorner = XMVector3TransformCoord(worldCorner, lightView);

			const _float x = XMVectorGetX(lightSpaceCorner);
			const _float y = XMVectorGetY(lightSpaceCorner);
			const _float z = XMVectorGetZ(lightSpaceCorner);

			minZ = min(minZ, z);
			maxZ = max(maxZ, z);
		}

		const _float minX = -radius;
		const _float maxX = radius;
		const _float minY = -radius;
		const _float maxY = radius;

		const _float casterDepthPadding = max(kMinShadowCascadeCasterDepthPadding, radius * kShadowCascadeCasterDepthPaddingScale);
		const _float nearZ = 0.f;
		const _float farZ = max(maxZ + casterDepthPadding, nearZ + kMinCascadeDepthRange);

		_matrix lightProj = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, nearZ, farZ);
		ApplyShadowTexelSnapping(lightView, lightProj, ResolveCascadeTileResolution(atlasScaleOffset.x));

		outCascade = {};
		XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&outCascade.view), lightView);
		XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&outCascade.proj), lightProj);
		outCascade.splitDepth = splitFar;
		outCascade.atlasScaleOffset = atlasScaleOffset;
		outCascade.lightSpaceBounds = _float4(minX, maxX, minY, maxY);
		outCascade.lightSpaceDepthRange = _float4(nearZ, farZ, 0.f, 0.f);
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

void CLight::BuildDirectionalShadow(CCamera* _cam, _float _shadowDistance, ShadowMatrices& _outShadowMatix)
{
	if (!_cam)
		return;

	_outShadowMatix = {};

	vector3 camPos = _cam->GetTransform()->Get_Position();
	vector3 camFwd = _cam->GetTransform()->Get_Directions().forward;
	camFwd = camFwd.normalized();

	vector3 center = camPos + camFwd * (_shadowDistance * 0.5f);

	vector3 lightDir = GetTransform()->Get_Directions().forward;
	lightDir = lightDir.normalized();

	vector3 lightPos = center - lightDir * _shadowDistance;

	_vector eye = XMVectorSet(lightPos.x, lightPos.y, lightPos.z, 1.f);
	_vector at = XMVectorSet(center.x, center.y, center.z, 1.f);

	_vector up = ResolveShadowUpVector(lightDir);

	_matrix V = XMMatrixLookAtLH(eye, at, up);

	_float half = _shadowDistance * 0.5f;
	_float nearZ = 0.0f;
	_float farZ = _shadowDistance * 2.0f;

	_matrix P = XMMatrixOrthographicOffCenterLH(-half, half, -half, half, nearZ, farZ);

	XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&_outShadowMatix.view), V);
	XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&_outShadowMatix.proj), P);
	_outShadowMatix.cascadeCount = min(kDefaultShadowCascadeCount, kMaxShadowCascades);
	_outShadowMatix.shadowDistance = _shadowDistance;
	_outShadowMatix.splitLambda = (_cam->GetViewMode() == CCamera::ViewMode::Perspective) ? kDefaultShadowSplitLambda : 0.f;

	const _float cameraNear = max(_cam->GetNear(), 0.001f);
	const _float desiredShadowFar = max(_shadowDistance, cameraNear + kMinCascadeDepthRange);
	const _float cameraFar = max(cameraNear + kMinCascadeDepthRange, min(_cam->GetFar(), desiredShadowFar));

	_float previousSplit = cameraNear;

	for (_uint cascadeIndex = 0u; cascadeIndex < _outShadowMatix.cascadeCount; ++cascadeIndex)
	{
		_float splitFar = cameraFar;

		if (_cam->GetViewMode() == CCamera::ViewMode::Perspective)
		{
			const _float cascadeFactor = static_cast<_float>(cascadeIndex + 1u) / static_cast<_float>(_outShadowMatix.cascadeCount);
			const _float uniformSplit = cameraNear + (cameraFar - cameraNear) * cascadeFactor;
			const _float logSplit = cameraNear * powf(cameraFar / cameraNear, cascadeFactor);
			splitFar = uniformSplit + (logSplit - uniformSplit) * _outShadowMatix.splitLambda;
		}
		else
		{
			const _float cascadeFactor = static_cast<_float>(cascadeIndex + 1u) / static_cast<_float>(_outShadowMatix.cascadeCount);
			splitFar = cameraNear + (cameraFar - cameraNear) * cascadeFactor;
		}

		splitFar = max(splitFar, previousSplit + kMinCascadeDepthRange);
		splitFar = min(splitFar, cameraFar);

		XMFLOAT3 frustumCorners[8] = {};
		if (_cam->GetViewMode() == CCamera::ViewMode::Perspective)
			BuildPerspectiveCascadeCorners(_cam, previousSplit, splitFar, frustumCorners);
		else
			BuildOrthographicCascadeCorners(_cam, previousSplit, splitFar, frustumCorners);

		ShadowCascadeMatrix& cascade = _outShadowMatix.cascades[cascadeIndex];
		BuildCascadeShadowMatrix(frustumCorners, lightDir, splitFar, BuildCascadeAtlasScaleOffset(cascadeIndex), cascade);

		previousSplit = splitFar;
	}

	for (_uint cascadeIndex = _outShadowMatix.cascadeCount; cascadeIndex < kMaxShadowCascades; ++cascadeIndex)
	{
		ShadowCascadeMatrix& cascade = _outShadowMatix.cascades[cascadeIndex];
		XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&cascade.view), V);
		XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&cascade.proj), P);
		cascade.splitDepth = cameraFar;
		cascade.atlasScaleOffset = BuildCascadeAtlasScaleOffset(cascadeIndex);
		cascade.lightSpaceBounds = _float4(-half, half, -half, half);
		cascade.lightSpaceDepthRange = _float4(nearZ, farZ, 0.f, 0.f);
	}
}


