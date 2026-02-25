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

	vector3 center = _cam->Get_Transform()->Get_Position();
	_float3 sceneMin = { FLT_MAX, FLT_MAX, FLT_MAX };
	_float3 sceneMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	_bool hasSceneBounds = false;

	if (CGameObject* camObject = _cam->Get_GameObject())
	{
		if (CScene* scene = camObject->Get_Scene())
		{
			vector<CRenderer*> renderers = scene->Get_MeshObjects();
			for (auto* renderer : renderers)
			{
				if (!renderer || !renderer->Get_GameObject() || !renderer->Get_Transform())
					continue;
				if (!renderer->Get_GameObject()->IsRecursiveActive() || !renderer->Get_Enable())
					continue;
				if (!renderer->IsCastShadow())
					continue;

				CMeshBuffer* meshBuffer = renderer->Get_MeshBuffer();
				if (!meshBuffer)
					continue;

				const BoundingBox& localBox = meshBuffer->Get_Info().boundingBox;
				BoundingOrientedBox localObb = {};
				BoundingOrientedBox::CreateFromBoundingBox(localObb, localBox);

				BoundingOrientedBox worldObb = {};
				localObb.Transform(worldObb, renderer->Get_Transform()->Get_WorldMatrix());

				XMFLOAT3 corners[8] = {};
				worldObb.GetCorners(corners);

				for (_int i = 0; i < 8; ++i)
				{
					sceneMin.x = min(sceneMin.x, corners[i].x);
					sceneMin.y = min(sceneMin.y, corners[i].y);
					sceneMin.z = min(sceneMin.z, corners[i].z);

					sceneMax.x = max(sceneMax.x, corners[i].x);
					sceneMax.y = max(sceneMax.y, corners[i].y);
					sceneMax.z = max(sceneMax.z, corners[i].z);
				}

				hasSceneBounds = true;
			}
		}
	}

	_float extX = _shadowDistance * 0.5f;
	_float extY = _shadowDistance * 0.5f;
	_float extZ = _shadowDistance;
	if (hasSceneBounds)
	{
		center = vector3(
			(sceneMin.x + sceneMax.x) * 0.5f,
			(sceneMin.y + sceneMax.y) * 0.5f,
			(sceneMin.z + sceneMax.z) * 0.5f
		);

		extX = max((sceneMax.x - sceneMin.x) * 0.5f, 1.f);
		extY = max((sceneMax.y - sceneMin.y) * 0.5f, 1.f);
		extZ = max((sceneMax.z - sceneMin.z) * 0.5f, 1.f);
	}

	vector3 lightDir = Get_Transform()->Get_Directions().forward;
	lightDir = lightDir.normalized();

	const _float radius = max(extZ, max(extX, extY));
	vector3 lightPos = center - lightDir * (radius * 2.f);

	_vector eye = XMVectorSet(lightPos.x, lightPos.y, lightPos.z, 1.f);
	_vector at = XMVectorSet(center.x, center.y, center.z, 1.f);

	_vector worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	_vector ldir = XMVector3Normalize(XMVectorSet(lightDir.x, lightDir.y, lightDir.z, 0.f));

	_float upDot = fabsf(XMVectorGetX(XMVector3Dot(ldir, worldUp)));
	_vector up = (upDot > 0.99f) ? XMVectorSet(0.f, 0.f, 1.f, 0.f) : worldUp;

	_matrix V = XMMatrixLookAtLH(eye, at, up);

	_float minX = -extX;
	_float maxX = extX;
	_float minY = -extY;
	_float maxY = extY;
	_float minZ = 0.f;
	_float maxZ = radius * 4.f;

	if (hasSceneBounds)
	{
		XMFLOAT3 corners[8] =
		{
			{ sceneMin.x, sceneMin.y, sceneMin.z },
			{ sceneMax.x, sceneMin.y, sceneMin.z },
			{ sceneMin.x, sceneMax.y, sceneMin.z },
			{ sceneMax.x, sceneMax.y, sceneMin.z },
			{ sceneMin.x, sceneMin.y, sceneMax.z },
			{ sceneMax.x, sceneMin.y, sceneMax.z },
			{ sceneMin.x, sceneMax.y, sceneMax.z },
			{ sceneMax.x, sceneMax.y, sceneMax.z }
		};

		minX = FLT_MAX;
		minY = FLT_MAX;
		minZ = FLT_MAX;
		maxX = -FLT_MAX;
		maxY = -FLT_MAX;
		maxZ = -FLT_MAX;

		for (_int i = 0; i < 8; ++i)
		{
			_vector p = XMVectorSet(corners[i].x, corners[i].y, corners[i].z, 1.f);
			_vector lv = XMVector3TransformCoord(p, V);
			_float x = XMVectorGetX(lv);
			_float y = XMVectorGetY(lv);
			_float z = XMVectorGetZ(lv);

			minX = min(minX, x);
			maxX = max(maxX, x);
			minY = min(minY, y);
			maxY = max(maxY, y);
			minZ = min(minZ, z);
			maxZ = max(maxZ, z);
		}

		const _float pad = 10.f;
		minX -= pad;
		maxX += pad;
		minY -= pad;
		maxY += pad;
		minZ = max(0.f, minZ - pad);
		maxZ += pad;
	}

	if (maxZ <= minZ)
		maxZ = minZ + 1.f;

	_matrix P = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);

	XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&_outShadowMatix.view), V);
	XMStoreFloat4x4(reinterpret_cast<_float4x4*>(&_outShadowMatix.proj), P);
}
