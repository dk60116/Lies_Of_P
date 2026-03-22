#include "epch.h"
#include "RectTransform.h"

namespace
{
    vector2 ProjectRectLocalPointToScreen(const CRectTransform& rect, const vector2& localPoint)
    {
        const _vector local = XMVectorSet(localPoint.x, localPoint.y, 0.f, 1.f);
        const _vector world = XMVector3TransformCoord(local, rect.Get_WorldMatrix());
        const _matrix viewMatrix = XMMatrixTranslation(-50.f, -50.f, 0.f);

        const _float aspect = CDisplay::GetInstance().Get_Aspect();
        const _float halfHeight = 7.2f * 0.5f;
        const _float halfWidth = halfHeight * aspect;
        const _matrix projMatrix = XMMatrixOrthographicOffCenterLH
        (
            -halfWidth, halfWidth,
            -halfHeight, halfHeight,
            0.f, 1.f
        );

        _vector clipPos = XMVector4Transform(world, viewMatrix);
        clipPos = XMVector4Transform(clipPos, projMatrix);

        const _float clipW = XMVectorGetW(clipPos);
        if (fabsf(clipW) < 1e-6f)
            return vector2::zero();

        const _float ndcX = XMVectorGetX(clipPos) / clipW;
        const _float ndcY = XMVectorGetY(clipPos) / clipW;
        const vector2 screenResolution = vector2(CDisplay::GetInstance().Get_ScreenResolution().x, CDisplay::GetInstance().Get_ScreenResolution().y);

        return vector2
        (
            (ndcX * 0.5f + 0.5f) * screenResolution.x,
            (ndcY * 0.5f + 0.5f) * screenResolution.y
        );
    }
}

CRectTransform::CRectTransform()
    : m_pUI(nullptr)
    , m_vAnchoredPosition({})
    , m_vAnchoredScale({})
    , m_vStaticWH({})
    , m_vSizeScale(vector3::one())
    , m_fWidth(0.f)
    , m_fHeight(0.f)
    , m_sAnchors({})
    , m_vPivot(vector2::one() * 0.5f)
    , m_pParentRect(nullptr)
    , m_bIsRootRect(true)
{
    m_strName = L"Rect Transform";
}

CRectTransform::~CRectTransform()
{
}

CRectTransform* CRectTransform::Create()
{
    return new CRectTransform();
}

CComponent* CRectTransform::Clone() const
{
    CRectTransform* clone = new CRectTransform();

    clone->m_vPosition = this->m_vPosition;
    clone->m_vQuaternion = this->m_vQuaternion;
    clone->m_vScale = this->m_vScale;
    clone->m_vAnchoredPosition = this->m_vAnchoredPosition;
    clone->m_vAnchoredScale = this->m_vAnchoredScale;
    clone->m_vStaticWH = this->m_vStaticWH;
    clone->m_vSizeScale = this->m_vSizeScale;
    clone->m_fWidth = this->m_fWidth;
    clone->m_fHeight = this->m_fHeight;
    clone->m_sAnchors = this->m_sAnchors;
    clone->m_vPivot = this->m_vPivot;

    if (this->m_pParent)
        clone->SetParent(this->m_pParent);

    return clone;
}

HRESULT CRectTransform::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

    return S_OK;
}

void CRectTransform::Update()
{
    CCanvas* canvas = m_pUI ? m_pUI->Get_Canvas() : nullptr;
    if (!canvas && m_pParent)
        canvas = m_pParent->Find_ComponentParentRecursive<CCanvas>();

    if (!canvas && !m_pParentRect)
    {
        __super::Update();
        return;
    }

    RefreshSizeFromLayout();
    SyncAnchoredPositionFromLocal();

    __super::Update();
}

void CRectTransform::Update_Editor()
{
    CCanvas* canvas = m_pUI ? m_pUI->Get_Canvas() : nullptr;
    if (!canvas && m_pParent)
        canvas = m_pParent->Find_ComponentParentRecursive<CCanvas>();

    if (!canvas && !m_pParentRect)
    {
        __super::Update_Editor();
        return;
    }

    RefreshSizeFromLayout();
    SyncAnchoredPositionFromLocal();

    __super::Update_Editor();
}

void CRectTransform::Render_Gizmo()
{
    CEditor& editor = CEditor::GetInstance();
    const _bool isSelected = (editor.Get_SelectedGameObject() == m_pGameObject);

    CCanvas* canvas = m_pUI ? m_pUI->Get_Canvas() : nullptr;
    if (!canvas && m_pParent)
        canvas = m_pParent->Find_ComponentParentRecursive<CCanvas>();

    vector2 canvasSize = {};

    CCamera* editorCam = CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera();
    if (!editorCam)
        return;

    _matrix viewMatrix = editorCam->GetViewMatrix();
    _matrix projMatrix = editorCam->GetProjectionMatrix();

    const _matrix rectWorldMatrix = XMLoadFloat4x4(&m_vMatWorld);
    _matrix gizmoWorldMatrix = rectWorldMatrix;

    vector2 pivotTrans = {};

    if (m_pUI && m_pUI->Is_Canvas())
    {
    }
    else if (!m_pParentRect)
    {
        if (canvas)
            canvasSize = vector2(canvas->GetTransform()->Get_LocalScale().x, canvas->GetTransform()->Get_LocalScale().y);

        pivotTrans = vector2(m_vPivot.x * m_vScale.x * canvasSize.x - m_vAnchoredScale.x * 0.5f, m_vPivot.y * m_vScale.y * canvasSize.y - m_vAnchoredScale.y * 0.5f);
        _matrix translateMat = XMMatrixTranslation(pivotTrans.x, pivotTrans.y, 0.f);
        gizmoWorldMatrix *= translateMat;
    }
    else
    {
        pivotTrans = vector2(m_vPivot.x * m_vScale.x * m_pParentRect->m_fWidth * 0.01f - m_vAnchoredScale.x * 0.5f, m_vPivot.y * m_vScale.y * m_pParentRect->m_fHeight * 0.01f - m_vAnchoredScale.y * 0.5f);
        _matrix translateMat = XMMatrixTranslation(pivotTrans.x, pivotTrans.y, 0.f);
        gizmoWorldMatrix *= translateMat;
    }

    if (m_pUI && m_pUI->m_pLineMat && m_pUI->m_pRectGizmoMesh)
    {
        const _float4 lineColor = isSelected
            ? _float4(1.f, 220.f / 255.f, 120.f / 255.f, 1.f)
            : _float4(180.f / 255.f, 220.f / 255.f, 1.f, 190.f / 255.f);

        m_pUI->m_pLineMat->Set_BaseColor(lineColor);
        m_pUI->m_pLineMat->Bind_Matrix(rectWorldMatrix);
        m_pUI->m_pLineMat->Bind_Camera(_float3(), viewMatrix, projMatrix, 0);
        m_pUI->m_pRectGizmoMesh->Render();
    }

    if (!isSelected)
        return;

    const D3D11_VIEWPORT* vp = CGraphicDevice::GetInstance().Get_CurrentViewport();

    _float world[16];
    memcpy(world, &gizmoWorldMatrix, sizeof(float) * 16);

    _float view[16];
    memcpy(view, &viewMatrix, sizeof(float) * 16);

    _float projection[16];
    memcpy(projection, &projMatrix, sizeof(float) * 16);

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::BeginFrame();
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuizmo::AllowAxisFlip(false);

    if (vp)
    {
        ImGuizmo::SetRect
        (
            vp->TopLeftX,
            vp->TopLeftY,
            vp->Width,
            vp->Height
        );
    }
    else
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    }

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;

    CEditor::TransformControleTool mode = editor.Get_ControleTool();

    if (mode == CEditor::TransformControleTool::MOVE)
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (mode == CEditor::TransformControleTool::ROTATE)
        currentGizmoOperation = ImGuizmo::ROTATE;
    if (mode == CEditor::TransformControleTool::SCALE)
        currentGizmoOperation = ImGuizmo::SCALE;

    _bool manipulated = ImGuizmo::Manipulate
    (
        view,
        projection,
        currentGizmoOperation,
        ImGuizmo::LOCAL,
        world
    );

    if (manipulated)
    {
        _matrix newWorldMatrix = XMLoadFloat4x4(reinterpret_cast<const _float4x4*>(world));

        _matrix translateMat = XMMatrixTranslation(-pivotTrans.x, -pivotTrans.y, 0.f);
        newWorldMatrix *= translateMat;

        const _bool hasCanvas = (m_pUI && m_pUI->Get_Canvas());
        const vector3 previousScale = m_vScale;

        if (m_pParent)
        {
            // Convert world to local using the parent's inverse matrix.
            _matrix parentInv = XMMatrixInverse(nullptr, m_pParent->Get_WorldMatrix());
            // Build the local matrix under the parent.
            _matrix localMatrix = newWorldMatrix * parentInv;
            // Decompose local transform components.
            _vector S, R, T;
            XMMatrixDecompose(&S, &R, &T, localMatrix);

            vector3 decomposedScale = {};
            XMStoreFloat3(reinterpret_cast<_float3*>(&decomposedScale), S);
            XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), R);
            XMStoreFloat3(reinterpret_cast<_float3*>(&m_vPosition), T);

            if (hasCanvas)
            {
                if (currentGizmoOperation == ImGuizmo::SCALE)
                {
                    const _float baseScaleX = (fabsf(m_vSizeScale.x) > 1e-4f) ? (previousScale.x / m_vSizeScale.x) : previousScale.x;
                    const _float baseScaleY = (fabsf(m_vSizeScale.y) > 1e-4f) ? (previousScale.y / m_vSizeScale.y) : previousScale.y;
                    m_vSizeScale.x = (fabsf(baseScaleX) > 1e-4f) ? (decomposedScale.x / baseScaleX) : decomposedScale.x;
                    m_vSizeScale.y = (fabsf(baseScaleY) > 1e-4f) ? (decomposedScale.y / baseScaleY) : decomposedScale.y;
                    m_vSizeScale.z = decomposedScale.z;
                }

                RefreshSizeFromLayout();
                SyncAnchoredPositionFromLocal();
            }
            else
            {
                m_vScale = decomposedScale;
            }
        }
        else
        {
            _vector S, R, T;
            XMMatrixDecompose(&S, &R, &T, newWorldMatrix);

            vector3 decomposedScale = {};
            XMStoreFloat3(reinterpret_cast<_float3*>(&decomposedScale), S);
            XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), R);
            XMStoreFloat3(reinterpret_cast<_float3*>(&m_vPosition), T);

            if (hasCanvas)
            {
                if (currentGizmoOperation == ImGuizmo::SCALE)
                {
                    const _float baseScaleX = (fabsf(m_vSizeScale.x) > 1e-4f) ? (previousScale.x / m_vSizeScale.x) : previousScale.x;
                    const _float baseScaleY = (fabsf(m_vSizeScale.y) > 1e-4f) ? (previousScale.y / m_vSizeScale.y) : previousScale.y;
                    m_vSizeScale.x = (fabsf(baseScaleX) > 1e-4f) ? (decomposedScale.x / baseScaleX) : decomposedScale.x;
                    m_vSizeScale.y = (fabsf(baseScaleY) > 1e-4f) ? (decomposedScale.y / baseScaleY) : decomposedScale.y;
                    m_vSizeScale.z = decomposedScale.z;
                }

                RefreshSizeFromLayout();
                SyncAnchoredPositionFromLocal();
            }
            else
            {
                m_vScale = decomposedScale;
            }
        }

        __super::Update_Editor();
    }
}

void CRectTransform::OnDestroy()
{
    __super::OnDestroy();

    Safe_Release(m_pParentRect);
    m_pParentRect = nullptr;

    Safe_Release(m_pUI);
    m_pUI = nullptr;
}

void CRectTransform::Set_UI(CUI* _pUI)
{
    if (m_pUI == _pUI)
        return;

    CCanvas* prevCanvas = (m_pUI) ? m_pUI->Get_Canvas() : nullptr;
    if (prevCanvas && m_pUI)
        prevCanvas->Remove_UIObject(m_pUI);

    if (m_pUI)
        m_pUI->Set_Canvas(nullptr);

    Safe_Release(m_pUI);
    m_pUI = _pUI;

    if (!m_pUI)
        return;

    m_pUI->AddRef();

    CCanvas* canvas = nullptr;
    if (m_pParent)
        canvas = m_pParent->Find_ComponentParentRecursive<CCanvas>();

    if (canvas)
    {
        m_pUI->Set_Canvas(canvas);
        canvas->Add_UIObject(m_pUI);
    }
}

void CRectTransform::SetParent(CTransform* _parent)
{
    CCanvas* prevCanvas = (m_pUI) ? m_pUI->Get_Canvas() : nullptr;
    const _bool needsDefaultLayout = NeedsDefaultLayoutInitialization();

    __super::SetParent(_parent);

    Safe_Release(m_pParentRect);
    m_pParentRect = nullptr;
    m_bIsRootRect = true;

    if (!_parent)
    {
        if (prevCanvas && m_pUI)
            prevCanvas->Remove_UIObject(m_pUI);

        if (m_pUI)
            m_pUI->Set_Canvas(nullptr);

        return;
    }

    CCanvas* canvas = _parent->Find_ComponentParentRecursive<CCanvas>();

    if (prevCanvas && prevCanvas != canvas && m_pUI)
        prevCanvas->Remove_UIObject(m_pUI);

    if (!canvas)
    {
        if (m_pUI)
            m_pUI->Set_Canvas(nullptr);

        Update();
        return;
    }

    if (m_pUI)
    {
        m_pUI->Set_Canvas(canvas);
        canvas->Add_UIObject(m_pUI);
    }

    if (m_pParent && !m_pParent->Get_GameObject()->GetComponent<CCanvas>())
    {
        m_pParentRect = dynamic_cast<CRectTransform*>(_parent);
        m_bIsRootRect = (m_pParentRect == nullptr);

        if (m_pParentRect)
            m_pParentRect->AddRef();
    }

    if (needsDefaultLayout)
    {
        Set_WidthHeight(100.f, 100.f);
        Set_AnchoredPosition(0.f, 0.f);
        return;
    }

    Update();
}

void CRectTransform::Set_LocalScale(const vector3& _scale)
{
    Set_SizeScale(_scale);
}

void CRectTransform::Set_LocalScale(const _float _x, const _float _y, const _float _z)
{
    Set_SizeScale(_x, _y, _z);
}

void CRectTransform::Set_LocalScale(const _float _value)
{
    Set_SizeScale(_value, _value, _value);
}

void CRectTransform::Set_LocalScaleX(const _float _value)
{
    vector3 scale = Get_SizeScale();
    scale.x = _value;
    Set_SizeScale(scale);
}

void CRectTransform::Set_LocalScaleY(const _float _value)
{
    vector3 scale = Get_SizeScale();
    scale.y = _value;
    Set_SizeScale(scale);
}

void CRectTransform::Set_LocalScaleZ(const _float _value)
{
    vector3 scale = Get_SizeScale();
    scale.z = _value;
    Set_SizeScale(scale);
}

void CRectTransform::Add_LocalScale(const vector3& _scale)
{
    Set_SizeScale(Get_SizeScale() + _scale);
}

void CRectTransform::Add_LocalScaleX(const _float _value)
{
    vector3 scale = Get_SizeScale();
    scale.x += _value;
    Set_SizeScale(scale);
}

void CRectTransform::Add_LocalScaleY(const _float _value)
{
    vector3 scale = Get_SizeScale();
    scale.y += _value;
    Set_SizeScale(scale);
}

void CRectTransform::Add_LocalScaleZ(const _float _value)
{
    vector3 scale = Get_SizeScale();
    scale.z += _value;
    Set_SizeScale(scale);
}

const vector2 CRectTransform::Get_AnchoredPosition() const
{
    return m_vAnchoredPosition;
}

void CRectTransform::Set_AnchoredPosition(const vector2 _pos)
{
    const vector2 referenceSize = GetReferenceSize();
    if (!m_pParentRect && referenceSize == vector2::zero())
        return;

    RefreshSizeFromLayout();
    m_vAnchoredPosition = _pos;
    SyncLocalPositionFromAnchored();

    Update();
}

void CRectTransform::Set_AnchoredPosition(const _float _x, const _float _y)
{
    Set_AnchoredPosition(vector2(_x, _y));
}

void CRectTransform::Set_AnchoredPositionX(const _float _value)
{
    Set_AnchoredPosition(vector2(_value, m_vAnchoredPosition.y));
}

void CRectTransform::Set_AnchoredPositionY(const _float _value)
{
    Set_AnchoredPosition(vector2(m_vAnchoredPosition.x, _value));
}

const vector2 CRectTransform::Get_AnchoredSize() const
{
    const vector2 referenceSize = GetReferenceSize();
    const vector2 anchorSpan = GetAnchorSpan();

    return vector2
    (
        referenceSize.x * anchorSpan.x + m_vStaticWH.x,
        referenceSize.y * anchorSpan.y + m_vStaticWH.y
    );
}

void CRectTransform::Set_AnchoredSize(const vector2 _size)
{
    const vector2 referenceSize = GetReferenceSize();
    if (!m_pParentRect && referenceSize == vector2::zero())
        return;

    const vector2 currentAnchoredPosition = m_vAnchoredPosition;
    const vector2 anchorSpan = GetAnchorSpan();

    m_vStaticWH = vector2
    (
        _size.x - referenceSize.x * anchorSpan.x,
        _size.y - referenceSize.y * anchorSpan.y
    );
    RefreshSizeFromLayout();
    m_vAnchoredPosition = currentAnchoredPosition;
    SyncLocalPositionFromAnchored();

    Update();
}

void CRectTransform::Set_AnchoredSize(const _float _x, const _float _y)
{
    Set_AnchoredSize(vector2(_x, _y));

    Update();
}

void CRectTransform::Set_AnchoredSizeX(const _float _x)
{
    Set_AnchoredSize(vector2(_x, m_fHeight));
}

void CRectTransform::Set_AnchoredSizeY(const _float _y)
{
    Set_AnchoredSize(vector2(m_fWidth, _y));
}

const vector3 CRectTransform::Get_SizeScale() const
{
    return m_vSizeScale;
}

void CRectTransform::Set_SizeScale(const vector3& _scale)
{
    m_vSizeScale = _scale;

    const vector2 referenceSize = GetReferenceSize();

    if (!m_pParentRect && referenceSize == vector2::zero())
    {
        __super::Set_LocalScale(_scale);
        return;
    }

    const vector2 currentAnchoredPosition = m_vAnchoredPosition;

    RefreshSizeFromLayout();
    m_vAnchoredPosition = currentAnchoredPosition;
    SyncLocalPositionFromAnchored();

    Update();
}

void CRectTransform::Set_SizeScale(const _float _x, const _float _y, const _float _z)
{
    Set_SizeScale(vector3(_x, _y, _z));
}

const vector2 CRectTransform::Get_ScreenPosition() const
{
    CCanvas* canvas = m_pUI ? m_pUI->Get_Canvas() : nullptr;

    if (!canvas)
        return vector2::zero();

    return ProjectRectLocalPointToScreen(*this, vector2(m_vPivot.x - 0.5f, m_vPivot.y - 0.5f));
}

const vector2 CRectTransform::Get_ScreenCenterPosition() const
{
    CCanvas* canvas = m_pUI ? m_pUI->Get_Canvas() : nullptr;

    if (!canvas)
        return vector2::zero();

    return ProjectRectLocalPointToScreen(*this, vector2::zero());
}

const _float CRectTransform::Get_Width() const
{
    return m_fWidth;
}

const _float CRectTransform::Get_Height() const
{
    return m_fHeight;
}

const vector2 CRectTransform::Get_WidthHeight() const
{
    return vector2(m_fWidth, m_fHeight);
}

const vector2 CRectTransform::Get_Pivot() const
{
    return m_vPivot;
}

void CRectTransform::Set_Pivot(vector2 _pivot)
{
    _pivot.x = clamp(_pivot.x, 0.f, 1.f);
    _pivot.y = clamp(_pivot.y, 0.f, 1.f);

    m_vPivot = _pivot;

    Update();
}

void CRectTransform::Set_Pivot(const _float _x, const _float _y)
{
    Set_Pivot(vector2(_x, _y));
}

void CRectTransform::Set_PivotX(_float _value)
{
    Set_Pivot(_value, m_vPivot.y);
}

void CRectTransform::Set_PivotY(_float _value)
{
    Set_Pivot(m_vPivot.x, _value);
}

const CRectTransform::Anchors& CRectTransform::Get_Anchors()
{
    return m_sAnchors;
}

void CRectTransform::Set_AnchorsMin(const vector2 _pivot)
{
    const vector2 currentSize = Get_WidthHeight();
    const vector2 currentAnchoredPosition = Get_AnchoredPosition();
    const _float sizeScaleX = (fabsf(m_vSizeScale.x) > 1e-4f) ? m_vSizeScale.x : 1.f;
    const _float sizeScaleY = (fabsf(m_vSizeScale.y) > 1e-4f) ? m_vSizeScale.y : 1.f;
    const vector2 referenceSize = GetReferenceSize();

    m_sAnchors.min = _pivot;

    if (m_pParentRect || referenceSize != vector2::zero())
    {
        const vector2 anchorSpan = GetAnchorSpan();

        m_vStaticWH = vector2
        (
            (currentSize.x / sizeScaleX) - referenceSize.x * anchorSpan.x,
            (currentSize.y / sizeScaleY) - referenceSize.y * anchorSpan.y
        );
        RefreshSizeFromLayout();
        m_vAnchoredPosition = currentAnchoredPosition;
        SyncLocalPositionFromAnchored();
    }

    Update();
}

void CRectTransform::Set_AnchorsMin(const _float _x, const _float _y)
{
    Set_AnchorsMin(vector2(_x, _y));
}

void CRectTransform::Set_AnchorsMax(const vector2 _pivot)
{
    const vector2 currentSize = Get_WidthHeight();
    const vector2 currentAnchoredPosition = Get_AnchoredPosition();
    const _float sizeScaleX = (fabsf(m_vSizeScale.x) > 1e-4f) ? m_vSizeScale.x : 1.f;
    const _float sizeScaleY = (fabsf(m_vSizeScale.y) > 1e-4f) ? m_vSizeScale.y : 1.f;
    const vector2 referenceSize = GetReferenceSize();

    m_sAnchors.max = _pivot;

    if (m_pParentRect || referenceSize != vector2::zero())
    {
        const vector2 anchorSpan = GetAnchorSpan();

        m_vStaticWH = vector2
        (
            (currentSize.x / sizeScaleX) - referenceSize.x * anchorSpan.x,
            (currentSize.y / sizeScaleY) - referenceSize.y * anchorSpan.y
        );
        RefreshSizeFromLayout();
        m_vAnchoredPosition = currentAnchoredPosition;
        SyncLocalPositionFromAnchored();
    }

    Update();
}

void CRectTransform::Set_AnchorsMax(const _float _x, const _float _y)
{
    Set_AnchorsMax(vector2(_x, _y));
}

void CRectTransform::Set_WidthHeight(const vector2 _rect)
{
    const vector2 referenceSize = GetReferenceSize();
    if (!m_pParentRect && referenceSize == vector2::zero())
        return;

    const vector2 currentAnchoredPosition = m_vAnchoredPosition;
    const vector2 anchorSpan = GetAnchorSpan();
    const _float sizeScaleX = (fabsf(m_vSizeScale.x) > 1e-4f) ? m_vSizeScale.x : 1.f;
    const _float sizeScaleY = (fabsf(m_vSizeScale.y) > 1e-4f) ? m_vSizeScale.y : 1.f;

    m_vStaticWH = vector2
    (
        (_rect.x / sizeScaleX) - referenceSize.x * anchorSpan.x,
        (_rect.y / sizeScaleY) - referenceSize.y * anchorSpan.y
    );
    RefreshSizeFromLayout();
    m_vAnchoredPosition = currentAnchoredPosition;
    SyncLocalPositionFromAnchored();

    Update();
}

void CRectTransform::Set_WidthHeight(const _float _x, const _float _y)
{
    Set_WidthHeight(vector2(_x, _y));
}

void CRectTransform::Set_WidthHeight(const _int _x, const _int _y)
{
    Set_WidthHeight(vector2(_x, _y));
}

void CRectTransform::Set_WidthHeight(const _int _wh)
{
	Set_WidthHeight(_wh, _wh);
}

void CRectTransform::Set_Width(const _float _value)
{
    Set_WidthHeight(_value, m_fHeight);
}

void CRectTransform::Set_Width(const _int _value)
{
    Set_WidthHeight(static_cast<_float>(_value), m_fHeight);
}

void CRectTransform::Set_Height(const _float _value)
{
    Set_WidthHeight(m_fWidth, _value);
}

void CRectTransform::Set_Height(const _int _value)
{
    Set_WidthHeight(m_fWidth, static_cast<_float>(_value));
}

vector2 CRectTransform::GetReferenceSize() const
{
    if (m_pParentRect)
        return vector2(m_pParentRect->m_fWidth, m_pParentRect->m_fHeight);

    CCanvas* canvas = nullptr;
    if (m_pUI)
        canvas = m_pUI->Get_Canvas();

    if (!canvas && m_pParent)
        canvas = m_pParent->Find_ComponentParentRecursive<CCanvas>();

    if (!canvas)
        return vector2::zero();

    return vector2(canvas->GetTransform()->Get_LocalScale().x, canvas->GetTransform()->Get_LocalScale().y) * 100.f;
}

vector2 CRectTransform::GetAnchorSpan() const
{
    return vector2(m_sAnchors.max.x - m_sAnchors.min.x, m_sAnchors.max.y - m_sAnchors.min.y);
}

vector2 CRectTransform::GetAnchorReference() const
{
    const vector2 anchorSpan = GetAnchorSpan();

    return vector2
    (
        m_sAnchors.min.x + anchorSpan.x * m_vPivot.x,
        m_sAnchors.min.y + anchorSpan.y * m_vPivot.y
    );
}

void CRectTransform::RefreshSizeFromLayout()
{
    const vector2 referenceSize = GetReferenceSize();
    const vector2 anchorSpan = GetAnchorSpan();
    const _float baseWidth = referenceSize.x * anchorSpan.x + m_vStaticWH.x;
    const _float baseHeight = referenceSize.y * anchorSpan.y + m_vStaticWH.y;

    m_fWidth = baseWidth * m_vSizeScale.x;
    m_fHeight = baseHeight * m_vSizeScale.y;

    m_vScale.x = (referenceSize.x != 0.f) ? (m_fWidth / referenceSize.x) : 0.f;
    m_vScale.y = (referenceSize.y != 0.f) ? (m_fHeight / referenceSize.y) : 0.f;
    m_vScale.z = m_vSizeScale.z;
    m_vAnchoredScale = vector2(m_fWidth * 0.01f, m_fHeight * 0.01f);
}

void CRectTransform::SyncAnchoredPositionFromLocal()
{
    const vector2 referenceSize = GetReferenceSize();
    const vector2 anchorReference = GetAnchorReference();

    m_vAnchoredPosition.x = referenceSize.x * m_vPosition.x + m_fWidth * (m_vPivot.x - 0.5f);
    m_vAnchoredPosition.x += referenceSize.x * (0.5f - anchorReference.x);

    m_vAnchoredPosition.y = referenceSize.y * m_vPosition.y + m_fHeight * (m_vPivot.y - 0.5f);
    m_vAnchoredPosition.y += referenceSize.y * (0.5f - anchorReference.y);
}

void CRectTransform::SyncLocalPositionFromAnchored()
{
    const vector2 referenceSize = GetReferenceSize();
    const vector2 anchorReference = GetAnchorReference();

    if (referenceSize.x != 0.f)
        m_vPosition.x = (m_vAnchoredPosition.x - m_fWidth * (m_vPivot.x - 0.5f) - referenceSize.x * (0.5f - anchorReference.x)) / referenceSize.x;
    else
        m_vPosition.x = 0.f;

    if (referenceSize.y != 0.f)
        m_vPosition.y = (m_vAnchoredPosition.y - m_fHeight * (m_vPivot.y - 0.5f) - referenceSize.y * (0.5f - anchorReference.y)) / referenceSize.y;
    else
        m_vPosition.y = 0.f;
}

_bool CRectTransform::NeedsDefaultLayoutInitialization() const
{
    return m_vAnchoredPosition == vector2::zero()
        && m_vAnchoredScale == vector2::zero()
        && m_vStaticWH == vector2::zero()
        && m_vSizeScale == vector3::one()
        && m_fWidth == 0.f
        && m_fHeight == 0.f
        && m_sAnchors.min == vector2::one() * 0.5f
        && m_sAnchors.max == vector2::one() * 0.5f
        && m_vPivot == vector2::one() * 0.5f
        && m_vPosition == vector3::zero()
        && m_vScale == vector3::one();
}
