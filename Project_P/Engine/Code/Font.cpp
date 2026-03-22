#include "epch.h"
#include "Font.h"

#include <filesystem>

namespace fs = std::filesystem;

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

    wstring* path = nullptr;

    if (_desc)
        path = static_cast<wstring*>(_desc);

    if (path)
    {
        fs::path fontPath(*path);

        if (!fontPath.is_absolute())
        {
            wchar_t exePath[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            fontPath = fs::path(exePath).parent_path() / fontPath;
        }

        error_code ec;
        const fs::path absoluteFontPath = fs::absolute(fontPath, ec);
        if (ec || !fs::exists(absoluteFontPath))
        {
            CDebug::LogError(L"Initialize Font Failed - missing spritefont: " + fontPath.wstring());
            return E_FAIL;
        }

        try
        {
            m_pSpriteFont = new SpriteFont(device, absoluteFontPath.c_str());
        }
        catch (const std::exception& ex)
        {
            CDebug::LogError(L"Initialize Font Failed - SpriteFont exception: " + absoluteFontPath.wstring());
            CDebug::LogError(CEngineString::StringToWString(ex.what()));
            return E_FAIL;
        }
    }

    if (!m_pSpriteFont)
    {
        CDebug::LogError(L"Initialize Font Failed: " + (path ? *path : wstring(L"<null>")));
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
