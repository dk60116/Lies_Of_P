#include "epch.h"
#include "RectTransform.h"

CRectTransform::CRectTransform()
    : m_pUI(nullptr)
    , m_vAnchoredPosition({})
    , m_vAnchoredScale({})
    , m_vStaticWH({})
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
    __super::Update();

    CCanvas* canvas = m_pUI->Get_Canvas();
    
    if (!canvas)
        return;

    vector2 canvasSize(canvas->Get_Transform()->Get_LocalScale().x,
        canvas->Get_Transform()->Get_LocalScale().y);

    if (!m_pParentRect)
    {
        m_fWidth = canvasSize.x * 100.f * m_vScale.x;
        m_fHeight = canvasSize.y * 100.f * m_vScale.y;

        m_vAnchoredPosition.x = canvasSize.x * 100.f * m_vPosition.x + m_fWidth * (m_vPivot.x - 0.5f);
        m_vAnchoredPosition.x += canvasSize.x * 100.f * (0.5f - m_sAnchors.min.x);

        m_vAnchoredPosition.y = canvasSize.y * 100.f * m_vPosition.y + m_fHeight * (m_vPivot.y - 0.5f);
        m_vAnchoredPosition.y += canvasSize.y * 100.f * (0.5f - m_sAnchors.min.y);
    }
    else
    {
        m_fWidth = m_pParentRect->m_fWidth * m_vScale.x;
        m_fHeight = m_pParentRect->m_fHeight * m_vScale.y;

        m_vAnchoredPosition.x = m_pParentRect->m_fWidth * m_vPosition.x + m_fWidth * (m_vPivot.x - 0.5f);
        m_vAnchoredPosition.x += m_pParentRect->m_fWidth * (0.5f - m_sAnchors.min.x);

        m_vAnchoredPosition.y = m_pParentRect->m_fHeight * m_vPosition.y + m_fHeight * (m_vPivot.y - 0.5f);
        m_vAnchoredPosition.y += m_pParentRect->m_fHeight * (0.5f - m_sAnchors.min.y);
    }

    m_vAnchoredScale = vector2(m_fWidth * 0.01f, m_fHeight * 0.01f);
}

void CRectTransform::Render_Gizmo()
{
    if (CEditor::GetInstance().Get_SelectedGameObject() != m_pGameObject)
        return;

    CCanvas* canvas = m_pUI->Get_Canvas();

    vector2 canvasSize = {};

    CCamera* editorCam = CSceneManager::GetInstance().Get_CrtScene()->Get_EditorCamera();

    _matrix viewMatrix = editorCam->Get_ViewMatrix();
    _matrix projMatrix = editorCam->Get_ProjectionMatrix();

    _matrix worldMatrix = XMLoadFloat4x4(&m_vMatWorld);

    vector2 pivotTrans = {};

    if (m_pUI->Is_Canvas())
    {
    }
    else if (!m_pParentRect)
    {
        canvasSize = vector2(canvas->Get_Transform()->Get_LocalScale().x, canvas->Get_Transform()->Get_LocalScale().y);

        pivotTrans = vector2(m_vPivot.x * m_vScale.x * canvasSize.x - m_vAnchoredScale.x * 0.5f, m_vPivot.y * m_vScale.y * canvasSize.y - m_vAnchoredScale.y * 0.5f);
        _matrix translateMat = XMMatrixTranslation(pivotTrans.x, pivotTrans.y, 0.f);
        worldMatrix *= translateMat;
    }
    else
    {
        pivotTrans = vector2(m_vPivot.x * m_vScale.x * m_pParentRect->m_fWidth * 0.01f - m_vAnchoredScale.x * 0.5f, m_vPivot.y * m_vScale.y * m_pParentRect->m_fHeight * 0.01f - m_vAnchoredScale.y * 0.5f);
        _matrix translateMat = XMMatrixTranslation(pivotTrans.x, pivotTrans.y, 0.f);
        worldMatrix *= translateMat;
    }

    _float world[16];
    memcpy(world, &worldMatrix, sizeof(float) * 16);

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

    CEditor& editor = CEditor::GetInstance();
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

        if (m_pParent)
        {
            // 부모의 월드 행렬의 역행렬
            _matrix parentInv = XMMatrixInverse(nullptr, m_pParent->Get_WorldMatrix());
            // 로컬 행렬 구하기
            _matrix localMatrix = newWorldMatrix * parentInv;

            // 로컬 위치/회전/스케일 추출
            _vector S, R, T;
            XMMatrixDecompose(&S, &R, &T, localMatrix);

            // 저장
            XMStoreFloat3(reinterpret_cast<_float3*>(&m_vScale), S);
            XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), R);
            XMStoreFloat3(reinterpret_cast<_float3*>(&m_vPosition), T);
        }
        else
        {
            // 부모 없으면 그냥 월드 == 로컬
            _vector S, R, T;
            XMMatrixDecompose(&S, &R, &T, newWorldMatrix);

            XMStoreFloat3(reinterpret_cast<_float3*>(&m_vScale), S);
            XMStoreFloat4(reinterpret_cast<_float4*>(&m_vQuaternion), R);
            XMStoreFloat3(reinterpret_cast<_float3*>(&m_vPosition), T);
        }
    }
}

void CRectTransform::OnDestroy()
{
    __super::OnDestroy();

    Safe_Release(m_pUI);
}

void CRectTransform::Set_UI(CUI* _pUI)
{
    m_pUI = _pUI;
    m_pUI->AddRef();
}

void CRectTransform::SetParent(CTransform* _parent)
{
    __super::SetParent(_parent);

    CCanvas* canvas = _parent->Find_ComponentParentRecursive<CCanvas>();

    if (canvas)
    {
        m_pUI->Set_Canvas(canvas);
        canvas->Add_UIObject(m_pUI);

        Safe_Release(m_pParentRect);

        if (m_pParent)
        {
            if (!m_pParent->Get_GameObject()->GetComponent<CCanvas>())
            {
                m_pParentRect = dynamic_cast<CRectTransform*>(_parent);
                m_bIsRootRect = false;
            }
            else
                m_bIsRootRect = true;

            if (m_pParentRect)
                m_pParentRect->AddRef();
        }

        Set_WidthHeight(100.f, 100.f);
        Set_AnchoredPosition(0.f, 0.f);
    }

    return;
}

const vector2 CRectTransform::Get_AnchoredPosition() const
{
    return m_vAnchoredPosition;
}

void CRectTransform::Set_AnchoredPosition(const vector2 _pos)
{
    CCanvas* canvas = m_pUI->Get_Canvas();

    if (!canvas)
        return;

    m_vAnchoredPosition = _pos;

    const vector2 canvasSize = vector2(canvas->Get_Transform()->Get_LocalScale().x, canvas->Get_Transform()->Get_LocalScale().y);

    if (!m_pParentRect)
    {
        const _float xA = canvasSize.x * 100.f;
        const _float yA = canvasSize.y * 100.f;

        if (xA != 0 && yA != 0)
        {
            m_vPosition.x = (m_vAnchoredPosition.x - m_fWidth * (m_vPivot.x - 0.5f) - xA * (0.5f - m_sAnchors.min.x)) / xA;
            m_vPosition.y = (m_vAnchoredPosition.y - m_fHeight * (m_vPivot.y - 0.5f) - yA * (0.5f - m_sAnchors.min.y)) / yA;
        }
        else
        {
            m_vPosition.x = 0.f;
            m_vPosition.y = 0.f;
        }
    }
    else
    {
        const _float xA = m_pParentRect->m_fWidth;
        const _float yA = m_pParentRect->m_fHeight;
        const _float xP = m_fWidth * (m_vPivot.x - 0.5f);
        const _float yP = m_fHeight * (m_vPivot.y - 0.5f);
        const _float xS = xA * (0.5f - m_sAnchors.min.x);
        const _float yS = yA * (0.5f - m_sAnchors.min.y);

        if (xA != 0 && yA != 0)
        {
            m_vPosition.x = (m_vAnchoredPosition.x - xP - xS) / xA;
            m_vPosition.y = (m_vAnchoredPosition.y - yP - yS) / yA;
        }
        else
        {
            m_vPosition.x = 0.f;
            m_vPosition.y = 0.f;
        }
    }

    Update();
}

void CRectTransform::Set_AnchoredPosition(const _float _x, const _float _y)
{
    Set_AnchoredPosition(vector2(_x, _y));
}

void CRectTransform::Set_AnchoredPositonX(const _float _value)
{
    Set_AnchoredPosition(vector2(_value, m_vAnchoredPosition.y));
}

void CRectTransform::Set_AnchoredPositonY(const _float _value)
{
    Set_AnchoredPosition(vector2(m_vAnchoredPosition.x, _value));
}

const vector2 CRectTransform::Get_ScreenPosition() const
{
    CCanvas* canvas = m_pUI->Get_Canvas();

    if (!canvas)
        return vector2::zero();

    const vector2 canvasSize = vector2(canvas->Get_Transform()->Get_LocalScale().x, canvas->Get_Transform()->Get_LocalScale().y) * 100.f;
    const vector2 pivotPos = vector2(canvasSize.x * m_vPivot.x, canvasSize.y * m_vPivot.y) + m_vAnchoredPosition;

    return pivotPos;
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
    m_sAnchors.min = _pivot;
    Update();
}

void CRectTransform::Set_AnchorsMin(const _float _x, const _float _y)
{
    m_sAnchors.min = vector2(_x, _y);
    Update();
}

void CRectTransform::Set_AnchorsMax(const vector2 _pivot)
{
    m_sAnchors.max = _pivot;
    Update();
}

void CRectTransform::Set_AnchorsMax(const _float _x, const _float _y)
{
    m_sAnchors.max = vector2(_x, _y);
    Update();
}

void CRectTransform::Set_WidthHeight(const vector2 _rect)
{
    CCanvas* canvas = m_pUI->Get_Canvas();

    if (!canvas)
        return;

    const vector2 canvasSize = vector2(canvas->Get_Transform()->Get_LocalScale().x, canvas->Get_Transform()->Get_LocalScale().y);
    const vector2 canvasResize = canvasSize * 100.f;

    const vector2 def = vector2(m_fWidth - _rect.x, m_fHeight - _rect.y);

    if (!m_pParentRect)
    {
        if (canvasResize.x != 0 && canvasResize.y != 0)
        {
            m_vScale.x = _rect.x / canvasResize.x;
            m_vScale.y = _rect.y / canvasResize.y;

            m_vPosition.x -= (def.x * (0.5f - m_vPivot.x)) / canvasResize.x;
            m_vPosition.y -= (def.y * (0.5f - m_vPivot.y)) / canvasResize.y;
        }
        else
        {
            m_vScale.x = 0;
            m_vScale.y = 0;
        }
    }
    else
    {
        if (m_pParentRect->m_fWidth != 0 && m_pParentRect->m_fHeight != 0)
        {
            m_vScale.x = (_rect.x) / m_pParentRect->m_fWidth;
            m_vScale.y = (_rect.y) / m_pParentRect->m_fHeight;

            m_vPosition.x -= (def.x * (0.5f - m_vPivot.x)) / m_pParentRect->m_fWidth;
            m_vPosition.y -= (def.y * (0.5f - m_vPivot.y)) / m_pParentRect->m_fHeight;
        }
        else
        {
            m_vScale.x = 0;
            m_vScale.y = 0;
        }
    }

    m_vStaticWH = _rect;

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
