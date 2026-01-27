#include "epch.h"
#include "Text.h"

CText::CText()
	: m_pFont(nullptr)
	, m_strText(L"")
    , m_fFontSize(10.f)
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

    if (this->m_pFont)
        clone->Set_Font(this->m_pFont);

    return clone;
}

HRESULT CText::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

    if (!m_pFont)
    {
        Set_Font(CResources::GetInstance().LoadOnGame<CFont>(L"Sans (Font)"));
    }

	return S_OK;
}

void CText::Render_Editor()
{
    m_pContext->OMSetDepthStencilState(CSceneManager::GetInstance().Get_CrtScene()->Get_UIStencillState(), 0);

    CCamera* cam = CSceneManager::GetInstance().Get_EditorCamera();

    _float3 camPos = _float3();
    _matrix matWorld = Get_Transform()->Get_WorldMatrix();
    _matrix matView = cam->Get_ViewMatrix();
    _matrix matProj = cam->Get_ProjectionMatrix();

    if (m_pGameObject != CEditor::GetInstance().Get_SelectedGameObject())
        return;

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

    CRectTransform* rect = Get_RectTransform();

    if (!rect)
        return;

    CCanvas* canvas = Get_Canvas();

    if (!canvas)
        return;

    const vector2 canvasSize = vector2(canvas->Get_Transform()->Get_LocalScale().x, canvas->Get_Transform()->Get_LocalScale().y) * 100.f;
    const vector2 screenPos = rect->Get_ScreenPosition();
    const vector2 scale = vector2(m_fFontSize, m_fFontSize);
    const vector2 pivot = rect->Get_Pivot();
    const _float rotation = rect->Get_LocalEulerAngles().z;
    const _vector textSizeVec = m_pFont->Get_SpriteFont()->MeasureString(m_strText.c_str());
    _float2 textSize = {};

    XMStoreFloat2(&textSize, textSizeVec);

    // 출력 위치
    _float2 pos;
    pos.x = screenPos.x;
    pos.y = canvasSize.y - screenPos.y;

    batch->Begin();

    m_pFont->Get_SpriteFont()->DrawString
    (
        batch,
        m_strText.c_str(),
        pos,
        m_vColor.toXMVector(),
        rotation,
        _float2(textSize.x * pivot.x, textSize.y * pivot.y),
        _float2(scale.x, scale.y) * 0.1f
    );

    batch->End();
}

void CText::OnDestroy()
{
    Safe_Release(m_pFont);
}

void CText::Set_Font(CFont* _font)
{
    if (m_pFont == _font)
        return;

    if (m_pFont)
        Safe_Release(m_pFont);

    m_pFont = _font;

    if (m_pFont)
        m_pFont->AddRef();
}

void CText::Set_FontSize(const _float _size)
{
    m_fFontSize = _size;
}

void CText::Set_Text(wstring _text)
{
    m_strText = _text;
}

void CText::Set_Text(string _text)
{
    m_strText = CEngineString::StringToWString(_text);
}
