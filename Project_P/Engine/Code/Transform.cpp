#include "epch.h"
#include "Transform.h"
#include "GameObject.h"
#include "LODGroup.h"
#include "RigidBody.h"

namespace
{
	thread_local _uint g_iThreadSnapshotSlot = 0u;

	_matrix BuildRotationGizmoMatrix(const _matrix& _worldMatrix)
	{
		_vector scale = {};
		_vector rotation = {};
		_vector translation = {};

		if (!XMMatrixDecompose(&scale, &rotation, &translation, _worldMatrix))
			return _worldMatrix;

		_float3 absScale = {};
		XMStoreFloat3(&absScale, scale);
		absScale.x = max(fabsf(absScale.x), 0.0001f);
		absScale.y = max(fabsf(absScale.y), 0.0001f);
		absScale.z = max(fabsf(absScale.z), 0.0001f);

		return XMMatrixScaling(absScale.x, absScale.y, absScale.z)
			* XMMatrixRotationQuaternion(rotation)
			* XMMatrixTranslationFromVector(translation);
	}

	void MarkAncestorLODGroupsDirty(CTransform* _transform)
	{
		CTransform* current = _transform;
		while (current)
		{
			CGameObject* owner = current->Get_GameObject();
			if (owner)
			{
				if (CLODGroup* lodGroup = owner->GetComponent<CLODGroup>())
					lodGroup->MarkRefreshNeeded();
			}

			current = current->Get_Parent();
		}
	}
}

CTransform::CTransform()
    : m_bIsRootParent(true)
    , m_pParent(nullptr)
    , m_lChildList({})
    , m_vPosition({})
    , m_vScale(vector3::one())
    , m_vEulerAngles({})
    , m_vQuaternion(quaternion::identity())
    , m_vWorldQuaternion(quaternion::identity())
    , m_vPrevQuaternion(quaternion::identity())
    , m_vPrevLocalQuat(quaternion::identity())
    , m_vMatWorld()
    , m_vMatLocal()
    , m_vMatLocalRotation()
    , m_vPrevPosition({})
    , m_vPrevEulerAngles({})
    , m_vPrevLoclaPos({})
    , m_vPrevLocalEuler({})
    , m_vPrevLocalScale({})
    , m_sDirections({})
    , m_sPrevDirections({})
{
    m_strName = L"Transform";
}

CTransform::~CTransform()
{
}

CTransform* CTransform::Create()
{
    return new CTransform();
}

CComponent* CTransform::Clone() const
{
    CTransform* clone = new CTransform();

    clone->m_bIsRootParent = this->m_bIsRootParent;
    clone->m_vPosition = this->m_vPosition;
    clone->m_vQuaternion = this->m_vQuaternion;
    clone->m_vScale = this->m_vScale;

    if (this->m_pParent)
        clone->SetParent(this->m_pParent);

    clone->Update();

    return clone;
}

HRESULT CTransform::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

    Bind_Matrix();
    Bind_Direction();
    for (_uint i = 0u; i < kSnapshotBufferCount; ++i)
        m_vSnapshotWorld[i] = m_vMatWorld;

    return S_OK;
}

void CTransform::Update()
{
    if (CSceneManager::GetInstance().IsPlaying())
    {
        if (m_pGameObject->IsStatic(CGameObject::STATIC_METHOD::TransformStatic))
            return;
    }

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Update_Editor()
{
    Bind_Matrix();
    Bind_Direction();
}

void CTransform::LateUpdate()
{
    m_vPrevPosition = m_vWorldPosition;
    m_vPrevEulerAngles = m_vWorldEulerAngles;
    m_vPrevLoclaPos = m_vPosition;
    m_vPrevLocalEuler = m_vEulerAngles;
    m_vPrevQuaternion = m_vWorldQuaternion;
    m_vPrevLocalQuat = m_vQuaternion;
    m_vPrevLocalScale = m_vScale;
    m_sPrevDirections = m_sDirections;
}

void CTransform::Render_Gizmo()
{
    CEditor& editor = CEditor::GetInstance();

    if (editor.Get_SelectedGameObject() != m_pGameObject)
        return;

    CCamera* editorCam = CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera();

    _matrix viewMatrix = editorCam->GetViewMatrix();
    _matrix projMatrix = editorCam->GetProjectionMatrix();

    _float view[16];
    memcpy(view, &viewMatrix, sizeof(float) * 16);

    _float projection[16];
    memcpy(projection, &projMatrix, sizeof(float) * 16);

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::BeginFrame();
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::AllowAxisFlip(false);

    const D3D11_VIEWPORT* vp = CGraphicDevice::GetInstance().Get_CurrentViewport();

    if (vp)
    {
        ImGuizmo::SetRect(vp->TopLeftX, vp->TopLeftY, vp->Width, vp->Height);
    }
    else
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    }

    const CEditor::TransformControleTool gizmoTool = editor.Get_GizmoControleTool();
    ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;

    if (gizmoTool == CEditor::TransformControleTool::ROTATE)
        gizmoOperation = ImGuizmo::ROTATE;
    else if (gizmoTool == CEditor::TransformControleTool::SCALE)
        gizmoOperation = ImGuizmo::SCALE;

    const _bool lockStaticGizmo = CSceneManager::GetInstance().IsPlaying() && m_pGameObject->IsStatic(CGameObject::STATIC_METHOD::TransformStatic);
    ImGuizmo::Enable(!lockStaticGizmo);

    const vector<CGameObject*>& multiSelected = editor.Get_MultiSelectedObjects();
    const bool isMultiSelect = multiSelected.size() > 1;

    _float world[16];

    _matrix gizmoPrev;

    if (isMultiSelect)
    {
        _float3 avgPos = { 0.f, 0.f, 0.f };
        for (CGameObject* obj : multiSelected)
        {
            if (!obj || !obj->GetTransform()) continue;
            const _float4x4& wm = obj->GetTransform()->m_vMatWorld;
            avgPos.x += wm._41;
            avgPos.y += wm._42;
            avgPos.z += wm._43;
        }
        float count = (float)multiSelected.size();
        avgPos.x /= count;
        avgPos.y /= count;
        avgPos.z /= count;

        gizmoPrev = XMMatrixTranslation(avgPos.x, avgPos.y, avgPos.z);
        memcpy(world, &gizmoPrev, sizeof(float) * 16);
    }
    else
    {
        gizmoPrev = XMLoadFloat4x4(&m_vMatWorld);
        if (gizmoOperation == ImGuizmo::ROTATE)
            gizmoPrev = BuildRotationGizmoMatrix(gizmoPrev);
        memcpy(world, &gizmoPrev, sizeof(float) * 16);
    }

    ImGuizmo::MODE gizmoMode = isMultiSelect ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    _bool manipulated = ImGuizmo::Manipulate(view, projection, gizmoOperation, gizmoMode, world);

    ImGuizmo::Enable(true);

    if (!manipulated)
        return;

    if (isMultiSelect)
    {
        _matrix gizmoNext = XMLoadFloat4x4(reinterpret_cast<const _float4x4*>(world));
        _matrix delta = XMMatrixMultiply(XMMatrixInverse(nullptr, gizmoPrev), gizmoNext);

        for (CGameObject* obj : multiSelected)
        {
            if (!obj) continue;
            CTransform* tr = obj->GetTransform();
            if (!tr) continue;

            _matrix objWorld = XMLoadFloat4x4(&tr->m_vMatWorld);
            _matrix newObjWorld = XMMatrixMultiply(objWorld, delta);

            if (CTransform* parent = tr->Get_Parent())
            {
                _matrix parentInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&parent->m_vMatWorld));
                _matrix localMatrix = XMMatrixMultiply(newObjWorld, parentInv);

                _vector S, R, T;
                XMMatrixDecompose(&S, &R, &T, localMatrix);
                XMStoreFloat3(reinterpret_cast<_float3*>(&tr->m_vScale), S);
                XMStoreFloat4(reinterpret_cast<_float4*>(&tr->m_vQuaternion), R);
                XMStoreFloat3(reinterpret_cast<_float3*>(&tr->m_vPosition), T);
            }
            else
            {
                _vector S, R, T;
                XMMatrixDecompose(&S, &R, &T, newObjWorld);
                XMStoreFloat3(reinterpret_cast<_float3*>(&tr->m_vScale), S);
                XMStoreFloat4(reinterpret_cast<_float4*>(&tr->m_vQuaternion), R);
                XMStoreFloat3(reinterpret_cast<_float3*>(&tr->m_vPosition), T);
            }
        }
    }
    else
    {
        _matrix newWorldMatrix = XMLoadFloat4x4(reinterpret_cast<const _float4x4*>(world));
        const vector3 preservedLocalPosition = m_vPosition;
        const vector3 preservedLocalScale = m_vScale;

        if (m_pParent)
        {
            _matrix parentInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_pParent->m_vMatWorld));
            _matrix localMatrix = newWorldMatrix * parentInv;

            _vector S, R, T;
            XMMatrixDecompose(&S, &R, &T, localMatrix);

            XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), R);
            if (gizmoOperation == ImGuizmo::ROTATE)
            {
                m_vPosition = preservedLocalPosition;
                m_vScale = preservedLocalScale;
            }
            else
            {
                XMStoreFloat3(reinterpret_cast<_float3*>(&m_vScale), S);
                XMStoreFloat3(reinterpret_cast<_float3*>(&m_vPosition), T);
            }
        }
        else
        {
            _vector S, R, T;
            XMMatrixDecompose(&S, &R, &T, newWorldMatrix);

            XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), R);
            if (gizmoOperation == ImGuizmo::ROTATE)
            {
                m_vPosition = preservedLocalPosition;
                m_vScale = preservedLocalScale;
            }
            else
            {
                XMStoreFloat3(reinterpret_cast<_float3*>(&m_vScale), S);
                XMStoreFloat3(reinterpret_cast<_float3*>(&m_vPosition), T);
            }
        }
    }
}

void CTransform::OnDestroy()
{
    const list<CTransform*> childList = m_lChildList;

    for (CTransform* child : childList)
    {
        if (!child)
            continue;

        if (child->m_pParent == this)
        {
            m_lChildList.remove(child);
            child->m_pParent = nullptr;
            child->m_bIsRootParent = true;
            CTransform* selfRef = this;
            Safe_Release(selfRef);
        }

        if (CGameObject* childObject = child->Get_GameObject())
            childObject->Destroy();
    }

    m_lChildList.clear();

    if (m_pParent)
    {
        CTransform* parent = m_pParent;
        m_pParent = nullptr;
        m_bIsRootParent = true;
        parent->m_lChildList.remove(this);
        Safe_Release(parent);
    }
}

CTransform* CTransform::Get_Parent() const
{
    return m_pParent;
}

void CTransform::SetParent(CTransform* _parent)
{
    if (_parent == m_pParent)
        return;

    CTransform* previousParent = m_pParent;
    _matrix W_old = XMLoadFloat4x4(&m_vMatWorld);

    if (m_pParent) {
        m_pParent->m_lChildList.remove(this);
        Safe_Release(m_pParent);
    }

    m_pParent = _parent;
    if (m_pParent) {
        m_pGameObject->Set_RecursiveActive(m_pParent->m_pGameObject->m_bRecursiveActive);
        m_pParent->m_lChildList.push_back(this);
        m_pParent->AddRef();
    }

    RecalcWorldUpChain(m_pParent);

    _matrix P = XMMatrixIdentity();
    if (m_pParent) P = XMLoadFloat4x4(&m_pParent->m_vMatWorld);
    _matrix invP = XMMatrixInverse(nullptr, P);

    _vector sW, rW, tW;
    _vector sP, rP, tP;
    _bool okW = XMMatrixDecompose(&sW, &rW, &tW, W_old);
    _bool okP = XMMatrixDecompose(&sP, &rP, &tP, P);

    auto safeDiv = [](float a, float b) { return (fabsf(b) < 1e-8f) ? 0.f : (a / b); };

    _float3 SW, SP;
    XMStoreFloat3(&SW, sW);
    XMStoreFloat3(&SP, sP);

    vector3 S_local(safeDiv(SW.x, SP.x),
        safeDiv(SW.y, SP.y),
        safeDiv(SW.z, SP.z));

    _vector rLocal = XMQuaternionMultiply(XMQuaternionInverse(rP), rW);
    rLocal = XMQuaternionNormalize(rLocal);

    vector3 T_local;
    {
        vector3 worldPos = vector3(m_vMatWorld._41, m_vMatWorld._42, m_vMatWorld._43);
        _vector wp = XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f);
        _vector lp = XMVector3TransformCoord(wp, invP);
        T_local = vector3(XMVectorGetX(lp), XMVectorGetY(lp), XMVectorGetZ(lp));
    }

    m_vScale = S_local;
    XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), rLocal);
    m_vPosition = T_local;

    Bind_Matrix();
    Bind_Direction();

    m_bIsRootParent = (m_pParent == nullptr);

    MarkAncestorLODGroupsDirty(previousParent);
    MarkAncestorLODGroupsDirty(this);
}

void CTransform::InsertChildBefore(CTransform* _child, CTransform* _beforeChild)
{
    if (!_child || _child->m_pParent != this)
        return;

    if (_beforeChild && _beforeChild->m_pParent != this)
        return;

    if (_child == _beforeChild)
        return;

    m_lChildList.remove(_child);

    if (!_beforeChild)
    {
        m_lChildList.push_back(_child);
        return;
    }

    auto it = find(m_lChildList.begin(), m_lChildList.end(), _beforeChild);
    if (it != m_lChildList.end())
        m_lChildList.insert(it, _child);
    else
        m_lChildList.push_back(_child);
}

const _bool CTransform::Is_Root() const
{
    return m_bIsRootParent;
}

CTransform* CTransform::Get_Child()
{
    if (m_lChildList.size() <= 0)
    {
        CDebug::LogError(L"Out of index - Get_Child: " + m_pGameObject->Get_ObjectNameID());
        return nullptr;
    }

    return m_lChildList.front();
}

CTransform* CTransform::Get_Child(const _int _index)
{
    _uint i = 0;

    for (TRAVERSAL_ITER(m_lChildList, it))
    {
        if (i == _index)
            return (*it);

        ++i;
    }

    return nullptr;
}

CTransform* CTransform::Find_Child(wstring _name)
{
    for (TRAVERSAL_ITER(m_lChildList, it))
    {
        if ((*it)->m_pGameObject->Get_ObjectName() == _name)
            return *it;
    }

    return nullptr;
}

CTransform* CTransform::Find_ChildRecursive(wstring _name)
{
    if (m_pGameObject->Get_ObjectName() == _name)
        return this;

    for (TRAVERSAL_ITER(m_lChildList, it))
    {
        CTransform* found = (*it)->Find_ChildRecursive(_name);

        if (found)
            return found;
    }

    return nullptr;
}

const list<CTransform*>& CTransform::Get_ChldList() const
{
    return m_lChildList;
}

const CTransform::DIRECTIONS& CTransform::Get_Directions()
{
    return m_sDirections;
}

const _matrix CTransform::Get_WorldMatrix() const
{
    _matrix mat = XMLoadFloat4x4(&m_vMatWorld);

    return mat;
}

const _matrix CTransform::GetSnapshotWorldMatrix() const
{
    return XMLoadFloat4x4(&m_vSnapshotWorld[g_iThreadSnapshotSlot % kSnapshotBufferCount]);
}

void CTransform::SnapshotWorldMatrix()
{
    m_vSnapshotWorld[g_iThreadSnapshotSlot % kSnapshotBufferCount] = m_vMatWorld;
}

void CTransform::SetThreadSnapshotSlot(const _uint _slot)
{
    g_iThreadSnapshotSlot = _slot % kSnapshotBufferCount;
}

void CTransform::ClearThreadSnapshotSlot()
{
    g_iThreadSnapshotSlot = 0u;
}

const _matrix CTransform::Get_LocalMatrix() const
{
    _matrix mat = XMLoadFloat4x4(&m_vMatLocal);

    return mat;
}

const _matrix CTransform::Get_InverseWorldMatrix() const
{
    _matrix mat = XMLoadFloat4x4(&m_vMatWorld);

    return XMMatrixInverse(nullptr, mat);
}

const vector3& CTransform::Get_Position()
{
    return m_vWorldPosition;
}

const vector3& CTransform::Get_LocalPosition()
{
    return m_vPosition;
}

const vector3 CTransform::Get_EulerAngles()
{
    if (!m_pParent)
        return m_vQuaternion.to_euler();
    else
    {
        _matrix worldMatrix = XMLoadFloat4x4(&m_vMatWorld);

        _vector scale;
        _vector rotationQuat;
        _vector translation;

        XMMatrixDecompose(&scale, &rotationQuat, &translation, worldMatrix);

        m_vWorldQuaternion = quaternion(rotationQuat);

        return m_vWorldQuaternion.to_euler();
    }

    return vector3::zero();
}

const vector3 CTransform::Get_LocalEulerAngles()
{
    return m_vQuaternion.to_euler();
}

vector3& CTransform::Get_LocalScale()
{
    return m_vScale;
}

const quaternion CTransform::Get_Quaternion() const
{
    return XMQuaternionRotationMatrix(XMLoadFloat4x4(&m_vMatWorld));
}

const quaternion& CTransform::Get_LocalQuaternion() const
{
    return m_vQuaternion;
}

void CTransform::Set_Position(const vector3& _pos)
{
    _matrix parentInv = XMMatrixIdentity();
    if (m_pParent)
        parentInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_pParent->m_vMatWorld));
    _matrix W = XMLoadFloat4x4(&m_vMatWorld);

    _vector S, R, T; XMMatrixDecompose(&S, &R, &T, W);
    W = XMMatrixScalingFromVector(S) * XMMatrixRotationQuaternion(R) * XMMatrixTranslation(_pos.x, _pos.y, _pos.z);
    SetTransformForMatrix(W);

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_Position(const _float _x, const _float _y, const _float _z)
{
    Set_Position(vector3(_x, _y, _z));
}

void CTransform::Set_PositionX(const _float _value)
{
    Set_Position(vector3(_value, m_vPosition.y, m_vPosition.z));
}

void CTransform::Set_PositionY(const _float _value)
{
    Set_Position(vector3(m_vPosition.x, _value, m_vPosition.z));
}

void CTransform::Set_PositionZ(const _float _value)
{
    Set_Position(vector3(m_vPosition.x, m_vPosition.y, _value));
}

void CTransform::Set_LocalPosition(const vector3& _pos)
{
    m_vPosition = _pos;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_LocalPosition(const _float _x, const _float _y, const _float _z)
{
    Set_LocalPosition(vector3(_x, _y, _z));
}

void CTransform::Set_LocalPositionX(const _float _value)
{
    m_vPosition.x = _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_LocalPositionY(const _float _value)
{
    m_vPosition.y = _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_LocalPositionZ(const _float _value)
{
    m_vPosition.z = _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_Position(const vector3& _value)
{
    m_vPosition += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_Position(const _float _x, const _float _y, const _float _z)
{
    m_vPosition += vector3(_x, _y, _z);

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Translate(const vector3& _value)
{
    if (CRigidBody* rigidBody = m_pGameObject->GetComponent<CRigidBody>())
    {
        if (rigidBody->Get_Enable())
        {
            rigidBody->Translate(_value);
            return;
        }
    }

    Add_Position(_value);

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Rotate(const vector3& _value)
{
    if (CRigidBody* rigidBody = m_pGameObject->GetComponent<CRigidBody>())
    {
        if (rigidBody->Get_Enable())
        {
            rigidBody->Rotate(_value);
            return;
        }
    }

    Add_EulerAngles(_value);

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_PositionX(const _float _value)
{
    m_vPosition.x += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_PositionY(const _float _value)
{
    m_vPosition.y += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_PositionZ(const _float _value)
{
    m_vPosition.z += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_LocalPosition(const vector3& _value)
{
    m_vPosition += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_LocalPosition(const _float _x, const _float _y, const _float _z)
{
    m_vPosition += vector3(_x, _y, _z);

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_LocalPositionX(const _float _value)
{
    m_vPosition.x += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_LocalPositionY(const _float _value)
{
    m_vPosition.y += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_LocalPositionZ(const _float _value)
{
    m_vPosition.z += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_Quaternion(const quaternion& _value)
{
    CTransform* tempParent = nullptr;

    if (m_pParent)
    {
        tempParent = m_pParent;
        SetParent(static_cast<CTransform*>(nullptr));
        Bind_Matrix();
    }

    m_vQuaternion = _value;

    if (tempParent)
        SetParent(tempParent);

    m_vEulerAngles = m_vQuaternion.to_euler();

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_LocalQuaternion(const quaternion& _value)
{
    m_vQuaternion = _value;

    m_vEulerAngles = m_vQuaternion.to_euler();

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_Quaternion(const quaternion& _delta)
{
    _vector q = XMLoadFloat4(reinterpret_cast<const _float4*>(&m_vQuaternion));
    _vector dq = XMLoadFloat4(reinterpret_cast<const _float4*>(&_delta));

    _vector result = XMQuaternionMultiply(dq, q);
    XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), XMQuaternionNormalize(result));

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_EulerAngles(const vector3& _rot)
{
    CTransform* tempParent = nullptr;

    if (m_pParent)
    {
        tempParent = m_pParent;
        SetParent(static_cast<CTransform*>(nullptr));
        Bind_Matrix();
    }

    Set_LocalEulerAngles(_rot);

    if (tempParent)
        SetParent(tempParent);

    m_vEulerAngles = _rot;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_EulerAngles(const _float _x, const _float _y, const _float _z)
{
    Set_EulerAngles(vector3(_x, _y, _z));
}

void CTransform::Set_EulerAnglesX(const _float _x)
{
    Set_EulerAngles(_x, m_vEulerAngles.y, m_vEulerAngles.z);
}

void CTransform::Set_EulerAnglesY(const _float _y)
{
    Set_EulerAngles(m_vEulerAngles.x, _y, m_vEulerAngles.z);
}

void CTransform::Set_EulerAnglesZ(const _float _z)
{
    Set_EulerAngles(m_vEulerAngles.x, m_vEulerAngles.y, _z);
}

void CTransform::Add_EulerAngles(const vector3& _rot)
{
    CTransform* tempParent = nullptr;

    if (m_pParent)
    {
        tempParent = m_pParent;
        SetParent(static_cast<CTransform*>(nullptr));
        Bind_Matrix();
    }

    Add_LocalEulerAngles(_rot);

    if (tempParent)
        SetParent(tempParent);

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_EulerAngles(const _float _x, const _float _y, const _float _z)
{
    Add_EulerAngles(vector3(_x, _y, _z));
}

void CTransform::Add_EulerAnglesX(const _float _value)
{
    Add_EulerAngles(vector3::right() * _value);
}

void CTransform::Add_EulerAnglesY(const _float _value)
{
    Add_EulerAngles(vector3::up() * _value);
}

void CTransform::Add_EulerAnglesZ(const _float _value)
{
    Add_EulerAngles(vector3::forward() * _value);
}

void CTransform::Set_LocalEulerAngles(const vector3& _rot)
{
    m_vQuaternion = _rot.to_quaternion();
}

void CTransform::Set_LocalEulerAngles(const _float _x, const _float _y, const _float _z)
{
    Set_LocalEulerAngles(vector3(_x, _y, _z));
}

void CTransform::Set_LocalEulerAnglesX(const _float _x)
{
    vector3 euler = Get_LocalEulerAngles();
    euler.x = _x;
    Set_LocalEulerAngles(euler);
}

void CTransform::Set_LocalEulerAnglesY(const _float _y)
{
    vector3 euler = Get_LocalEulerAngles();
    euler.y = _y;
    Set_LocalEulerAngles(euler);
}

void CTransform::Set_LocalEulerAnglesZ(const _float _z)
{
    vector3 euler = Get_LocalEulerAngles();
    euler.z = _z;
    Set_LocalEulerAngles(euler);
}

void CTransform::Add_LocalEulerAngles(const vector3& _rot)
{
    m_vEulerAngles += _rot;
    Set_LocalEulerAngles(m_vEulerAngles);
}

void CTransform::Add_LocalEulerAngles(const _float _x, const _float _y, const _float _z)
{
    Add_LocalEulerAngles(vector3(_x, _y, _z));
}

void CTransform::Add_LocalEulerAnglesX(const _float _value)
{
    Add_LocalEulerAngles(vector3(_value, 0.f, 0.f));
}

void CTransform::Add_LocalEulerAnglesY(const _float _value)
{
    Add_LocalEulerAngles(vector3(0.f, _value, 0.f));
}

void CTransform::Add_LocalEulerAnglesZ(const _float _value)
{
    Add_LocalEulerAngles(vector3(0.f, 0.f, _value));
}

void CTransform::Bind_Matrix()
{
    _matrix matScale = XMMatrixScaling(m_vScale.x, m_vScale.y, m_vScale.z);
    _matrix matRotation = XMMatrixRotationQuaternion(m_vQuaternion);
    _matrix matTranslation = XMMatrixTranslation(m_vPosition.x, m_vPosition.y, m_vPosition.z);

    _matrix matWorldF = XMMatrixIdentity();

    matWorldF *= matScale;
    matWorldF *= matRotation;
    matWorldF *= matTranslation;

    _matrix worldMat = {};

    if (m_pParent)
    {
        worldMat = matWorldF * XMLoadFloat4x4(&m_pParent->m_vMatWorld);
    }
    else
        worldMat = matWorldF;

    XMStoreFloat4x4(&m_vMatWorld, worldMat);

    m_vWorldPosition = vector3(m_vMatWorld._41, m_vMatWorld._42, m_vMatWorld._43);

    XMVECTOR S, Q, T;
    XMMatrixDecompose(&S, &Q, &T, worldMat);

    quaternion worldQ;
    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&worldQ), Q);

    vector3 eulerRad = worldQ.to_euler();
    m_vWorldQuaternion = worldQ;

    m_vWorldEulerAngles = worldQ.to_euler();
}

void CTransform::Bind_Direction()
{
    _matrix rotOnly = XMLoadFloat4x4(&m_vMatWorld);
    _vector forward = XMVector3TransformNormal(XMVectorSet(0.f, 0.f, 1.f, 0.f), rotOnly);
    _vector right = XMVector3TransformNormal(XMVectorSet(1.f, 0.f, 0.f, 0.f), rotOnly);
    _vector up = XMVector3TransformNormal(XMVectorSet(0.f, 1.f, 0.f, 0.f), rotOnly);

    XMFLOAT3 f, r, u;

    XMStoreFloat3(&f, forward);
    XMStoreFloat3(&r, right);
    XMStoreFloat3(&u, up);

    m_sDirections.forward = vector3(f.x, f.y, f.z).normalized();
    m_sDirections.back = -m_sDirections.forward;
    m_sDirections.right = vector3(r.x, r.y, r.z).normalized();
    m_sDirections.left = -m_sDirections.right;
    m_sDirections.up = vector3(u.x, u.y, u.z).normalized();
    m_sDirections.down = -m_sDirections.up;
}

void CTransform::RecalcWorldUpChain(CTransform* _t)
{
    if (!_t)
        return;
    if (_t->m_pParent)
        RecalcWorldUpChain(_t->m_pParent);
    _t->Bind_Matrix();
}

void CTransform::Set_LocalScale(const vector3& _scale)
{
    m_vScale = _scale;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_LocalScale(const _float _x, const _float _y, const _float _z)
{
    m_vScale = vector3(_x, _y, _z);

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_LocalScale(const _float _value)
{
    m_vScale = vector3::one() * _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_LocalScaleX(const _float _value)
{
    m_vScale.x = _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_LocalScaleY(const _float _value)
{
    m_vScale.y = _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Set_LocalScaleZ(const _float _value)
{
    m_vScale.z = _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_LocalScale(const vector3& _scale)
{
    m_vScale += _scale;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_LocalScaleX(const _float _value)
{
    m_vScale.x += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_LocalScaleY(const _float _value)
{
    m_vScale.y += _value;

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::Add_LocalScaleZ(const _float _value)
{
    m_vScale.z += _value;

    Bind_Matrix();
    Bind_Direction();
}

const vector3& CTransform::Get_PrevPosition()
{
    return m_vPrevPosition;
}

const vector3& CTransform::Get_PrevLocalPos()
{
    return m_vPrevLoclaPos;
}

const vector3& CTransform::Get_PrevEulerAngles()
{
    return m_vPrevEulerAngles;
}

const vector3& CTransform::Get_PrevLocalEuler()
{
    return m_vPrevLocalEuler;
}

const quaternion& CTransform::Get_PrevQuaternion()
{
    return m_vPrevQuaternion;
}

const quaternion& CTransform::Get_PrevLocalQuat()
{
    return m_vPrevLocalQuat;
}

void CTransform::SetTransformForMatrix(_matrix _matWorld)
{
    _matrix localMatrix = _matWorld;

    if (m_pParent)
    {
        _matrix parentInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_pParent->m_vMatWorld));
        localMatrix = _matWorld * parentInv;
    }

    _vector S, R, T;
    XMMatrixDecompose(&S, &R, &T, localMatrix);

    XMStoreFloat3(reinterpret_cast<_float3*>(&m_vScale), S);
    XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), R);
    XMStoreFloat3(reinterpret_cast<_float3*>(&m_vPosition), T);

    Bind_Matrix();
    Bind_Direction();
}

void CTransform::LookAt(const vector3& _target, const _uint _lockRotationFilter)
{
    vector3 upAxis = vector3(0, 1, 0);
    vector3 fwd = (_target - m_vWorldPosition).normalized();

    if (fabsf(fwd.dot(upAxis)) > 0.999f)
        upAxis = vector3(0, 0, 1);

    vector3 right = upAxis.cross(fwd).normalized();
    vector3 up = fwd.cross(right);

    _matrix rot =
    {
        right.x,  right.y,  right.z, 0,
        up.x,     up.y,     up.z,    0,
        fwd.x,    fwd.y,    fwd.z,   0,
        0,        0,        0,       1
    };

    _vector q = XMQuaternionRotationMatrix(rot);
    q = XMQuaternionNormalize(q);

    if (m_pParent)
    {
        _vector parentQ = m_pParent->m_vQuaternion.toXMVector();
        _vector invParentQ = XMQuaternionInverse(parentQ);
        q = XMQuaternionMultiply(invParentQ, q);
    }

    if (_lockRotationFilter)
    {
        quaternion qNew(q);                    
        quaternion qCur = m_vQuaternion;        

        vector3 eulerNew = qNew.to_euler();
        vector3 eulerCur = qCur.to_euler();

        if (_lockRotationFilter & 0x001) eulerNew.x = eulerCur.x;
        if (_lockRotationFilter & 0x010) eulerNew.y = eulerCur.y;
        if (_lockRotationFilter & 0x100) eulerNew.z = eulerCur.z;

        q = eulerNew.to_quaternion().toXMVector();
    }

    XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), XMQuaternionNormalize(q));

    m_vEulerAngles = m_vQuaternion.to_euler();

    Update();
}

const vector3 CTransform::LookRotation(const vector3& _target, const _uint _lockRotationFilter)
{
    vector3 forward = (_target - m_vWorldPosition).normalized();

    if (forward.lengthSq() < 1e-6f)
        return m_vQuaternion.to_euler();

    vector3 upAxis(0.f, 1.f, 0.f);

    if (fabsf(forward.dot(upAxis)) > 0.999f)
        upAxis = vector3(0.f, 0.f, 1.f);

    vector3 right = upAxis.cross(forward).normalized();
    upAxis = forward.cross(right);

    _matrix rotM =
    {
        right.x,    right.y,    right.z,    0.f,
        upAxis.x,   upAxis.y,   upAxis.z,   0.f,
        forward.x,  forward.y,  forward.z,  0.f,
        0.f,        0.f,        0.f,        1.f
    };

    _vector q = XMQuaternionRotationMatrix(rotM);
    q = XMQuaternionNormalize(q);

    if (m_pParent)
    {
        _vector parentQ = m_pParent->m_vQuaternion.toXMVector();
        _vector invParentQ = XMQuaternionInverse(parentQ);
        q = XMQuaternionMultiply(invParentQ, q);
    }

    if (_lockRotationFilter)
    {
        quaternion newQ;  XMStoreFloat4(reinterpret_cast<_float4*>(&newQ), q);
        vector3    eNew = newQ.to_euler();
        vector3    eCur = m_vQuaternion.to_euler();

        if (_lockRotationFilter & 0x001) eNew.x = eCur.x;
        if (_lockRotationFilter & 0x010) eNew.y = eCur.y;
        if (_lockRotationFilter & 0x100) eNew.z = eCur.z;

        return eNew;
    }

    quaternion finalQ;  XMStoreFloat4(reinterpret_cast<_float4*>(&finalQ), q);
    return finalQ.to_euler();
}

const quaternion CTransform::LookQuaternion(const vector3& _target, const _uint _lockRotationFilter)
{
    vector3 forward = (_target - m_vWorldPosition).normalized();

    if (forward.lengthSq() < 1e-6f)
        return m_vQuaternion;

    vector3 upAxis(0.f, 1.f, 0.f);

    if (fabsf(forward.dot(upAxis)) > 0.999f)
        upAxis = vector3(0.f, 0.f, 1.f);

    vector3 right = upAxis.cross(forward).normalized();
    upAxis = forward.cross(right);

    _matrix rotM =
    {
        right.x,    right.y,    right.z,    0.f,
        upAxis.x,   upAxis.y,   upAxis.z,   0.f,
        forward.x,  forward.y,  forward.z,  0.f,
        0.f,        0.f,        0.f,        1.f
    };

    _vector q = XMQuaternionRotationMatrix(rotM);
    q = XMQuaternionNormalize(q);

    if (m_pParent)
    {
        _vector parentQ = m_pParent->m_vWorldQuaternion.toXMVector();
        _vector invParentQ = XMQuaternionInverse(parentQ);
        q = XMQuaternionMultiply(invParentQ, q);
    }

    if (_lockRotationFilter)
    {
        quaternion newQ;  XMStoreFloat4(reinterpret_cast<_float4*>(&newQ), q);
        vector3    eNew = newQ.to_euler();
        vector3    eCur = m_vQuaternion.to_euler();

        if (_lockRotationFilter & 0x001)
            eNew.x = eCur.x;
        if (_lockRotationFilter & 0x010)
            eNew.y = eCur.y;
        if (_lockRotationFilter & 0x100)
            eNew.z = eCur.z;

        return eNew.to_quaternion();
    }

    quaternion finalQ;  XMStoreFloat4(reinterpret_cast<_float4*>(&finalQ), q);
    return finalQ;
}

void CTransform::RemoveChild(CTransform* _child)
{
    if (!_child)
        return;

    auto childIter = find(m_lChildList.begin(), m_lChildList.end(), _child);
    if (childIter == m_lChildList.end())
        return;

    m_lChildList.erase(childIter);

    if (_child->m_pParent == this)
    {
        _child->m_pParent = nullptr;
        _child->m_bIsRootParent = true;
        CTransform* selfRef = this;
        Safe_Release(selfRef);
    }
}
