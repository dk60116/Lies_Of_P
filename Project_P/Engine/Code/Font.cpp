#include "epch.h"
#include "Font.h"

CFont::CFont()
    : m_pSpriteFont(nullptr)
{
}

CFont::~CFont()
{
    OnDestroy();
}

CFont* CFont::Create()
{
    return new CFont();
}

HRESULT CFont::Initialize(const wstring& _name, const wstring& _filePath, void* _desc)
{
    if (FAILED(__super::Initialize(_name, _filePath, _desc)))
        return E_FAIL;

    ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
    ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

    wstring* path = nullptr;

    if (_desc)
        path = static_cast<wstring*>(_desc);

    if (path)
        m_pSpriteFont = new SpriteFont(device, (*path).c_str());

    if (!m_pSpriteFont)
    {
        CDebug::LogError(L"Initialize Font Failed: " + (*path));
        return E_FAIL;
    }

    return S_OK;
}

void CFont::OnDestroy()
{
    if (m_pSpriteFont)
    {
        delete m_pSpriteFont;
        m_pSpriteFont = nullptr;
    }
}

SpriteFont* CFont::Get_SpriteFont() const
{
    return m_pSpriteFont;
}
