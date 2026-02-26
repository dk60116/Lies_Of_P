#include "epch.h"
#include "Cloth.h"
#include "Physics.h"
#include "MeshRenderer.h"
#include "MeshFilter.h"
#include "MeshBuffer.h"
#include "SkinnedMeshRenderer.h"
#include "Material.h"
#include "Resources.h"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>

using namespace JPH;

CCloth::CCloth()
	: m_iSoftBodyID(BodyID())
	, m_bHasSoftBody(false)
	, m_bPendingCreate(false)
	, m_iGridWidth(12)
	, m_iGridHeight(10)
	, m_fGridSpacing(0.08f)
	, m_fTotalMass(1.5f)
	, m_fDamping(0.08f)
	, m_fCompliance(0.0002f)
	, m_bUseGravity(true)
	, m_bHasLastSyncedPosition(false)
	, m_vLastSyncedPosition(vector3::zero())
	, m_strTexturePath(L"")
{
	m_strName = L"Cloth";
}

CCloth::~CCloth()
{
}

CCloth* CCloth::Create()
{
	return new CCloth();
}

CComponent* CCloth::Clone() const
{
	CCloth* clone = new CCloth();
	clone->m_iGridWidth = m_iGridWidth;
	clone->m_iGridHeight = m_iGridHeight;
	clone->m_fGridSpacing = m_fGridSpacing;
	clone->m_fTotalMass = m_fTotalMass;
	clone->m_fDamping = m_fDamping;
	clone->m_fCompliance = m_fCompliance;
	clone->m_bUseGravity = m_bUseGravity;
	clone->m_bHasLastSyncedPosition = false;
	clone->m_vLastSyncedPosition = vector3::zero();
	clone->m_strTexturePath = m_strTexturePath;
	clone->m_bPendingCreate = true;
	return clone;
}

HRESULT CCloth::Initialize()
{
	ApplyTextureToRenderer();
	m_bPendingCreate = true;
	return S_OK;
}

void CCloth::Awake()
{
	if (m_bPendingCreate)
		CreateSoftBody();
}

void CCloth::OnEnable()
{
	if (!m_bHasSoftBody)
		CreateSoftBody();
}

void CCloth::OnDisable()
{
	DestroySoftBody();
}

void CCloth::FixedUpdate()
{
	if (!m_bHasSoftBody)
		return;

	if (!CPhysics::GetInstance().IsInitialized())
		return;

	if (!m_pGameObject || !m_pGameObject->Get_Transform())
		return;

	BodyInterface& bi = CPhysics::GetInstance().GetPhysicsSystem().GetBodyInterface();
	const vector3 currentTransformPos = m_pGameObject->Get_Transform()->Get_Position();

	if (m_bHasLastSyncedPosition)
	{
		const vector3 delta = currentTransformPos - m_vLastSyncedPosition;
		if (delta.lengthSq() > 0.0001f)
			bi.SetPosition(m_iSoftBodyID, RVec3(currentTransformPos.x, currentTransformPos.y, currentTransformPos.z), EActivation::Activate);
	}

	PhysicsSystem& ps = CPhysics::GetInstance().GetPhysicsSystem();
	BodyLockRead lock(ps.GetBodyLockInterface(), m_iSoftBodyID);
	if (lock.Succeeded())
	{
		const Body& body = lock.GetBody();
		const RVec3 pos = body.GetCenterOfMassPosition();
		m_vLastSyncedPosition = vector3((float)pos.GetX(), (float)pos.GetY(), (float)pos.GetZ());
		m_bHasLastSyncedPosition = true;
		m_pGameObject->Get_Transform()->Set_Position(m_vLastSyncedPosition);
	}

	vector<VertexTexNormalTangentBuffer> clothVertices;
	if (!BuildClothRenderVerticesFromSoftBody(clothVertices))
		return;

	if (CMeshRenderer* meshRenderer = m_pGameObject->GetComponent<CMeshRenderer>())
	{
		if (CMeshFilter* meshFilter = meshRenderer->Get_MeshFilter())
		{
			if (CMeshBuffer* meshBuffer = meshFilter->Get_MeshBuffer())
				meshBuffer->Update_VertexBuffer(clothVertices);
		}
	}
}

void CCloth::OnDestroy()
{
	DestroySoftBody();
}

void CCloth::SetTexturePath(const wstring& _path)
{
	m_strTexturePath = _path;
	ApplyTextureToRenderer();
}

const wstring& CCloth::GetTexturePath() const
{
	return m_strTexturePath;
}

void CCloth::RebuildClothBody()
{
	DestroySoftBody();
	CreateSoftBody();
}

_bool CCloth::GetUseGravity() const
{
	return m_bUseGravity;
}

void CCloth::SetUseGravity(const _bool _useGravity)
{
	m_bUseGravity = _useGravity;
	ApplyGravityToSoftBody();
}

CMaterial* CCloth::FindTargetMaterial()
{
	if (!m_pGameObject)
		return nullptr;

	if (CMeshRenderer* meshRenderer = m_pGameObject->GetComponent<CMeshRenderer>())
		return meshRenderer->Get_Material();

	if (CSkinnedMeshRenderer* skinnedMeshRenderer = m_pGameObject->GetComponent<CSkinnedMeshRenderer>())
		return skinnedMeshRenderer->Get_Material();

	return nullptr;
}

void CCloth::ApplyTextureToRenderer()
{
	CMaterial* material = FindTargetMaterial();
	if (!material)
		return;

	if (m_strTexturePath.empty())
		return;

	const string path = CEngineString::WStringToString(m_strTexturePath);
	const size_t pathHash = std::hash<string>{}(path);
	const wstring resourceName = CEngineString::StringToWString("ClothTexture/" + to_string(pathHash));

	CResources& resources = CResources::GetInstance();
	CTexture* texture = nullptr;
	auto found = resources.m_mGameResourceList.find(resourceName);
	if (found != resources.m_mGameResourceList.end())
		texture = dynamic_cast<CTexture*>(found->second);
	else
		texture = resources.CreateGameResource<CTexture>(resourceName, m_strTexturePath);

	if (texture)
		material->Set_Texture(texture, 0);
}

_bool CCloth::HasRendererTarget() const
{
	if (!m_pGameObject)
		return false;

	if (m_pGameObject->GetComponent<CMeshRenderer>())
		return true;
	if (m_pGameObject->GetComponent<CSkinnedMeshRenderer>())
		return true;
	return false;
}

void CCloth::CreateSoftBody()
{
	m_bPendingCreate = false;

	if (!m_pGameObject)
		return;

	if (!HasRendererTarget())
		return;

	if (!CPhysics::GetInstance().IsInitialized())
		return;

	const _uint width = (m_iGridWidth < 2) ? 2 : m_iGridWidth;
	const _uint height = (m_iGridHeight < 2) ? 2 : m_iGridHeight;
	const _float spacing = (m_fGridSpacing <= 0.001f) ? 0.001f : m_fGridSpacing;
	const _float mass = (m_fTotalMass <= 0.001f) ? 0.001f : m_fTotalMass;

	Ref<SoftBodySharedSettings> settings = new SoftBodySharedSettings();

	CMeshRenderer* meshRenderer = m_pGameObject->GetComponent<CMeshRenderer>();
	CMeshFilter* meshFilter = meshRenderer ? meshRenderer->Get_MeshFilter() : nullptr;
	CMeshBuffer* meshBuffer = meshFilter ? meshFilter->Get_MeshBuffer() : nullptr;
	vector<VertexTexNormalTangentBuffer> meshVertices = meshBuffer ? meshBuffer->Get_VertexBuffer() : vector<VertexTexNormalTangentBuffer>();
	vector<_uint> meshIndices = meshBuffer ? meshBuffer->Get_IndexBuffer() : vector<_uint>();

	const _float invMass = 1.0f / mass;
	if (!meshVertices.empty() && !meshIndices.empty() && (meshIndices.size() % 3 == 0))
	{
		settings->mVertices.reserve(meshVertices.size());
		settings->mFaces.reserve(meshIndices.size() / 3);

		_float maxY = -FLT_MAX;
		for (const auto& vtx : meshVertices)
			maxY = max(maxY, vtx.position.y);
		const _float pinThreshold = maxY - 0.02f;

		for (const auto& vtx : meshVertices)
		{
			const _float vertexInvMass = (vtx.position.y >= pinThreshold) ? 0.0f : invMass;
			settings->mVertices.emplace_back(Float3(vtx.position.x, vtx.position.y, vtx.position.z), Float3(0, 0, 0), vertexInvMass);
		}

		for (_uint i = 0; i + 2 < meshIndices.size(); i += 3)
			settings->mFaces.emplace_back(meshIndices[i + 0], meshIndices[i + 1], meshIndices[i + 2], 0);
	}
	else
	{
		settings->mVertices.reserve(width * height);
		settings->mFaces.reserve((width - 1) * (height - 1) * 2);

		for (_uint y = 0; y < height; ++y)
		{
			for (_uint x = 0; x < width; ++x)
			{
				_float3 p = _float3((float)x * spacing, -(float)y * spacing, 0.0f);
				settings->mVertices.emplace_back(Float3(p.x, p.y, p.z), Float3(0, 0, 0), invMass);
			}
		}

		auto idx = [width](_uint x, _uint y) { return y * width + x; };
		for (_uint y = 0; y + 1 < height; ++y)
		{
			for (_uint x = 0; x + 1 < width; ++x)
			{
				const _uint i0 = idx(x, y);
				const _uint i1 = idx(x + 1, y);
				const _uint i2 = idx(x, y + 1);
				const _uint i3 = idx(x + 1, y + 1);
				settings->mFaces.emplace_back(i0, i2, i1, 0);
				settings->mFaces.emplace_back(i1, i2, i3, 0);
			}
		}
	}

	SoftBodySharedSettings::VertexAttributes attrs[2];
	attrs[0].mCompliance = m_fCompliance;
	attrs[0].mShearCompliance = m_fCompliance;
	attrs[0].mBendCompliance = m_fCompliance * 2.0f;
	attrs[1] = attrs[0];
	settings->CreateConstraints(attrs, 2, SoftBodySharedSettings::EBendType::Distance);
	settings->Optimize();

	const vector3 pos = m_pGameObject->Get_Transform()->Get_Position();
	const quaternion rot = m_pGameObject->Get_Transform()->Get_Quaternion();
	SoftBodyCreationSettings softBodySettings(settings, RVec3(pos.x, pos.y, pos.z), Quat(rot.x, rot.y, rot.z, rot.w), Layers::MOVING);
	softBodySettings.mLinearDamping = m_fDamping;
	softBodySettings.mPressure = 0.0f;
	softBodySettings.mGravityFactor = m_bUseGravity ? 1.0f : 0.0f;
	softBodySettings.mNumIterations = 8;
	softBodySettings.mUpdatePosition = true;
	softBodySettings.mMakeRotationIdentity = true;
	softBodySettings.mAllowSleeping = false;
	softBodySettings.mUserData = 0;

	BodyInterface& bi = CPhysics::GetInstance().GetPhysicsSystem().GetBodyInterface();
	m_iSoftBodyID = bi.CreateAndAddSoftBody(softBodySettings, EActivation::Activate);
	m_bHasSoftBody = m_iSoftBodyID.IsInvalid() == false;
	m_vLastSyncedPosition = pos;
	m_bHasLastSyncedPosition = true;
}

void CCloth::ApplyGravityToSoftBody()
{
    if (!m_bHasSoftBody)
        return;

    if (!CPhysics::GetInstance().IsInitialized())
        return;

    PhysicsSystem& ps = CPhysics::GetInstance().GetPhysicsSystem();
    BodyLockWrite lock(ps.GetBodyLockInterface(), m_iSoftBodyID);
    if (!lock.Succeeded())
        return;

    Body& body = lock.GetBody();
    if (!body.IsSoftBody())
        return;

    SoftBodyMotionProperties* softMotion = static_cast<SoftBodyMotionProperties*>(body.GetMotionPropertiesUnchecked());
    if (!softMotion)
        return;

    softMotion->SetGravityFactor(m_bUseGravity ? 1.0f : 0.0f);
}

_bool CCloth::BuildClothRenderVerticesFromSoftBody(vector<VertexTexNormalTangentBuffer>& _outVertices)
{
    _outVertices.clear();

    if (!CPhysics::GetInstance().IsInitialized())
        return false;

    PhysicsSystem& ps = CPhysics::GetInstance().GetPhysicsSystem();
    BodyLockRead lock(ps.GetBodyLockInterface(), m_iSoftBodyID);
    if (!lock.Succeeded())
        return false;

    const Body& body = lock.GetBody();
    if (!body.IsSoftBody())
        return false;

    const SoftBodyMotionProperties* softMotion = static_cast<const SoftBodyMotionProperties*>(body.GetMotionPropertiesUnchecked());
    if (!softMotion)
        return false;

    if (!m_pGameObject)
        return false;

    CMeshRenderer* meshRenderer = m_pGameObject->GetComponent<CMeshRenderer>();
    if (!meshRenderer)
        return false;

    CMeshFilter* meshFilter = meshRenderer->Get_MeshFilter();
    if (!meshFilter)
        return false;

    CMeshBuffer* meshBuffer = meshFilter->Get_MeshBuffer();
    if (!meshBuffer)
        return false;

    vector<VertexTexNormalTangentBuffer> baseVertices = meshBuffer->Get_VertexBuffer();
    if (baseVertices.empty())
        return false;

    const auto& softVertices = softMotion->GetVertices();
    if (baseVertices.size() != softVertices.size())
        return false;

    for (_uint i = 0; i < baseVertices.size(); ++i)
    {
        const Vec3& p = softVertices[i].mPosition;
        baseVertices[i].position = _float3(p.GetX(), p.GetY(), p.GetZ());
    }

    _outVertices = move(baseVertices);
    return true;
}

void CCloth::DestroySoftBody()
{
	if (!m_bHasSoftBody)
		return;

	if (!CPhysics::GetInstance().IsInitialized())
	{
		m_bHasSoftBody = false;
		m_iSoftBodyID = BodyID();
		return;
	}

	BodyInterface& bi = CPhysics::GetInstance().GetPhysicsSystem().GetBodyInterface();
	if (!m_iSoftBodyID.IsInvalid())
	{
		bi.RemoveBody(m_iSoftBodyID);
		bi.DestroyBody(m_iSoftBodyID);
	}

	m_bHasSoftBody = false;
	m_iSoftBodyID = BodyID();
	m_bHasLastSyncedPosition = false;
	m_vLastSyncedPosition = vector3::zero();
}
