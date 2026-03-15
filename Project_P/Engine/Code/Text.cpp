#include "epch.h"
#include "Text.h"

namespace
{
    _float GetAlignedOffsetX(
        const CText::TextAligmentHorizontal horizontal,
        const _float rectWidth,
        const _float textWidth)
    {
        const _float maxOffset = max(0.f, rectWidth - textWidth) * 0.5f;

        switch (horizontal)
        {
        case CText::TextAligmentHorizontal::Left:
        case CText::TextAligmentHorizontal::JustFied:
        case CText::TextAligmentHorizontal::Flush:
            return -maxOffset;

        case CText::TextAligmentHorizontal::Right:
            return maxOffset;

        case CText::TextAligmentHorizontal::Center:
        default:
            return 0.f;
        }
    }

    _float GetAlignedOffsetY(
        const CText::TexAligmentVertical vertical,
        const _float rectHeight,
        const _float textHeight)
    {
        const _float maxOffset = max(0.f, rectHeight - textHeight) * 0.5f;

        switch (vertical)
        {
        case CText::TexAligmentVertical::Top:
            return -maxOffset;

        case CText::TexAligmentVertical::Bottom:
            return maxOffset;

        case CText::TexAligmentVertical::Middle:
        default:
            return 0.f;
        }
    }

    _float2 GetAlignedTextCenterOffset(
        const CText::TextAligmentHorizontal horizontal,
        const CText::TexAligmentVertical vertical,
        const _float rectWidth,
        const _float rectHeight,
        const _float textWidth,
        const _float textHeight)
    {
        return _float2
        (
            GetAlignedOffsetX(horizontal, rectWidth, textWidth),
            GetAlignedOffsetY(vertical, rectHeight, textHeight)
        );
    }

    _float2 RotateScreenOffset(const _float2& offset, const _float rotation)
    {
        const _float cosValue = cosf(rotation);
        const _float sinValue = sinf(rotation);

        return _float2
        (
            offset.x * cosValue - offset.y * sinValue,
            offset.x * sinValue + offset.y * cosValue
        );
    }

    bool ProjectWorldPointToViewport(
        const _vector worldPoint,
        const D3D11_VIEWPORT& viewport,
        const _fmatrix viewMatrix,
        const _fmatrix projectionMatrix,
        _float2& outScreenPosition,
        _float* outDepth = nullptr)
    {
        const _vector projected = XMVector3Project(
            worldPoint,
            viewport.TopLeftX,
            viewport.TopLeftY,
            viewport.Width,
            viewport.Height,
            viewport.MinDepth,
            viewport.MaxDepth,
            projectionMatrix,
            viewMatrix,
            XMMatrixIdentity());

        const _float depth = XMVectorGetZ(projected);
        if (outDepth)
            *outDepth = depth;

        outScreenPosition.x = XMVectorGetX(projected) - viewport.TopLeftX;
        outScreenPosition.y = XMVectorGetY(projected) - viewport.TopLeftY;

        return depth >= 0.f && depth <= 1.f;
    }

}

CText::CText()
    : m_pFont(nullptr)
    , m_strText(L"Text")
    , m_fFontSize(10.f)
    , m_eAlignmentHorizontal(TextAligmentHorizontal::Center)
    , m_eAlignmentVertical(TexAligmentVertical::Middle)
{
    m_strName = L"Text";
    m_vColor = ColorValue::black();
}

CText::~CText()
{
}

CText* CText::Create()
{
    return new CText();
}

CComponent* CText::Clone() const
{
    CText* clone = new CText();

    clone->m_strText = this->m_strText;
    clone->m_fFontSize = this->m_fFontSize;
    clone->m_eAlignmentHorizontal = this->m_eAlignmentHorizontal;
    clone->m_eAlignmentVertical = this->m_eAlignmentVertical;

    if (this->m_pFont)
        clone->SetFont(this->m_pFont);

    return clone;
}

HRESULT CText::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

    if (!m_pFont)
        SetFont(CResources::GetInstance().LoadOnGame<CFont>(L"Sans (Font)"));

    return S_OK;
}

void CText::Render_Editor()
{
}

void CText::Render_Gizmo()
{
    if (m_pGameObject != CEditor::GetInstance().Get_SelectedGameObject())
        return;

    CCamera* cam = CSceneManager::GetInstance().Get_EditorCamera();
    CScene* scene = CSceneManager::GetInstance().Get_CrtScene();

    if (!cam || !scene)
        return;

    _float3 camPos = _float3();
    _matrix matWorld = GetTransform()->Get_WorldMatrix();
    _matrix matView = cam->GetViewMatrix();
    _matrix matProj = cam->GetProjectionMatrix();

    m_pContext->OMSetDepthStencilState(scene->Get_UIStencillState(), 0);

    if (m_pLineMat)
    {
        m_pLineMat->Bind_Matrix(matWorld);
        m_pLineMat->Bind_Camera(camPos, matView, matProj, 0);
    }

    if (m_pRectGizmoMesh)
        m_pRectGizmoMesh->Render();
}

void CText::RenderText()
{
    if (!m_pFont || !m_pFont->Get_SpriteFont())
        return;

    SpriteBatch* batch = CGraphicDevice::GetInstance().Get_SpriteBatch();
    CRectTransform* rect = GetRectTransform();
    CCanvas* canvas = Get_Canvas();

    if (!batch || !rect || !canvas)
        return;

    const vector2 canvasSize = vector2(canvas->GetTransform()->Get_LocalScale().x, canvas->GetTransform()->Get_LocalScale().y) * 100.f;
    const vector2 screenPos = rect->Get_ScreenCenterPosition();
    const vector3 sizeScale3 = rect->Get_SizeScale();
    const vector2 scale = vector2(m_fFontSize * sizeScale3.x, m_fFontSize * sizeScale3.y);
    const _float rotation = rect->Get_LocalEulerAngles().z;
    const _vector textSizeVec = m_pFont->Get_SpriteFont()->MeasureString(m_strText.c_str());
    _float2 textSize = {};

    XMStoreFloat2(&textSize, textSizeVec);

    const _float2 fontScale = _float2(scale.x, scale.y) * 0.1f;
    const _float2 drawSize =
    {
        textSize.x * fontScale.x,
        textSize.y * fontScale.y
    };
    const _float2 alignmentOffset = GetAlignedTextCenterOffset(
        m_eAlignmentHorizontal,
        m_eAlignmentVertical,
        rect->Get_Width(),
        rect->Get_Height(),
        drawSize.x,
        drawSize.y);
    const _float2 rotatedOffset = RotateScreenOffset(alignmentOffset, rotation);

    _float2 pos = {};
    pos.x = screenPos.x + rotatedOffset.x;
    pos.y = canvasSize.y - screenPos.y + rotatedOffset.y;

    batch->Begin();

    m_pFont->Get_SpriteFont()->DrawString(
        batch,
        m_strText.c_str(),
        pos,
        m_vColor.toXMVector(),
        rotation,
        _float2(textSize.x * 0.5f, textSize.y * 0.5f),
        fontScale);

    batch->End();
}

void CText::RenderText_Editor()
{
    if (!m_pFont || !m_pFont->Get_SpriteFont())
        return;

    SpriteBatch* batch = CGraphicDevice::GetInstance().Get_SpriteBatch();
    CRectTransform* rect = GetRectTransform();
    CCanvas* canvas = Get_Canvas();
    CCamera* cam = CSceneManager::GetInstance().Get_EditorCamera();
    const D3D11_VIEWPORT* viewport = CGraphicDevice::GetInstance().Get_CurrentViewport();

    if (!batch || !rect || !canvas || !cam || !viewport)
        return;

    const vector3 sizeScale3 = rect->Get_SizeScale();
    const _float rectWidth = rect->Get_Width();
    const _float rectHeight = rect->Get_Height();
    const _matrix worldMatrix = GetTransform()->Get_WorldMatrix();
    const _matrix viewMatrix = cam->GetViewMatrix();
    const _matrix projectionMatrix = cam->GetProjectionMatrix();

    if (rectWidth <= 1e-4f || rectHeight <= 1e-4f)
        return;

    const _float localPixelX = 1.f / rectWidth;
    const _float localPixelY = 1.f / rectHeight;

    const _vector localCenter = XMVectorSet(0.f, 0.f, 0.f, 1.f);
    const _vector localOnePixelRight = XMVectorSet(localPixelX, 0.f, 0.f, 1.f);
    const _vector localOnePixelDown = XMVectorSet(0.f, -localPixelY, 0.f, 1.f);

    const _vector worldCenter = XMVector3TransformCoord(localCenter, worldMatrix);
    const _vector worldOnePixelRight = XMVector3TransformCoord(localOnePixelRight, worldMatrix);
    const _vector worldOnePixelDown = XMVector3TransformCoord(localOnePixelDown, worldMatrix);

    _float2 screenCenter = {};
    _float2 screenOnePixelRight = {};
    _float2 screenOnePixelDown = {};

    if (!ProjectWorldPointToViewport(worldCenter, *viewport, viewMatrix, projectionMatrix, screenCenter))
        return;

    if (!ProjectWorldPointToViewport(worldOnePixelRight, *viewport, viewMatrix, projectionMatrix, screenOnePixelRight))
        return;

    if (!ProjectWorldPointToViewport(worldOnePixelDown, *viewport, viewMatrix, projectionMatrix, screenOnePixelDown))
        return;

    const _float2 axisX = { screenOnePixelRight.x - screenCenter.x, screenOnePixelRight.y - screenCenter.y };
    const _float2 axisY = { screenOnePixelDown.x - screenCenter.x, screenOnePixelDown.y - screenCenter.y };
    const _float axisXLength = sqrtf(axisX.x * axisX.x + axisX.y * axisX.y);
    const _float axisYLength = sqrtf(axisY.x * axisY.x + axisY.y * axisY.y);

    if (axisXLength <= 1e-4f || axisYLength <= 1e-4f)
        return;

    const _vector textSizeVec = m_pFont->Get_SpriteFont()->MeasureString(m_strText.c_str());
    _float2 textSize = {};
    XMStoreFloat2(&textSize, textSizeVec);

    const _float2 fontScale =
    {
        m_fFontSize * sizeScale3.x * 0.1f,
        m_fFontSize * sizeScale3.y * 0.1f
    };
    const _float2 drawSize =
    {
        textSize.x * fontScale.x,
        textSize.y * fontScale.y
    };
    const _float2 alignmentOffset = GetAlignedTextCenterOffset
    (
        m_eAlignmentHorizontal,
        m_eAlignmentVertical,
        rectWidth,
        rectHeight,
        drawSize.x,
        drawSize.y
    );
    const _matrix transformMatrix =
    {
        axisX.x, axisX.y, 0.f, 0.f,
        axisY.x, axisY.y, 0.f, 0.f,
        0.f,     0.f,     1.f, 0.f,
        screenCenter.x, screenCenter.y, 0.f, 1.f
    };

    batch->Begin(SpriteSortMode_Deferred, nullptr, nullptr, nullptr, nullptr, nullptr, transformMatrix);

    m_pFont->Get_SpriteFont()->DrawString(
        batch,
        m_strText.c_str(),
        alignmentOffset,
        m_vColor.toXMVector(),
        0.f,
        _float2(textSize.x * 0.5f, textSize.y * 0.5f),
        fontScale);

    batch->End();
}

void CText::OnDestroy()
{
    __super::OnDestroy();

    Safe_Release(m_pFont);
}

void CText::SetFont(CFont* _font)
{
    if (m_pFont == _font)
        return;

    if (m_pFont)
        Safe_Release(m_pFont);

    m_pFont = _font;

    if (m_pFont)
        m_pFont->AddRef();
}

void CText::SetFontSize(const _float _size)
{
    m_fFontSize = _size;
}

void CText::SetText(const wstring& _text)
{
    m_strText = _text;
}

void CText::SetText(const string& _text)
{
    m_strText = CEngineString::StringToWString(_text);
}

void CText::ResetText()
{
    m_strText = L"";
}

CFont* CText::GetFont() const
{
    return m_pFont;
}

const wstring& CText::GetText() const
{
    return m_strText;
}

_float CText::GetFontSize() const
{
    return m_fFontSize;
}

CText::TextAligmentHorizontal CText::GetAligmentHorizontal() const
{
    return m_eAlignmentHorizontal;
}

CText::TexAligmentVertical CText::GetAligmentVertical() const
{
    return m_eAlignmentVertical;
}

void CText::SetAligmentHorizontal(const TextAligmentHorizontal _type)
{
    m_eAlignmentHorizontal = _type;
}

void CText::SetAligmentVertical(const TexAligmentVertical _type)
{
    m_eAlignmentVertical = _type;
}
