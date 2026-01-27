#pragma once

#include "UI.h"

NS_BEGIN(Engine)

class ENGINE_DLL CCanvas final : public CUI
{
    friend class CGameObject;

public:
    enum class RenderMode { ScreenSpace_Overlay, ScreenSpace_Camera, WorldSpace };

private:
    explicit CCanvas();
    ~CCanvas();

private:
    static CCanvas* Create();
    CComponent* Clone() const override;

public:
    HRESULT Initialize() override;
    void OnPreRender_Editor();
    void Render_Editor() override;
    void OnPostRender_Editor() override;
    void Render() override;
    void OnDestroy() override;

public:
    void Add_UIObject(CUI* _ui);
    const RenderMode Get_RenderMode() const;

private:
    RenderMode m_eRenderMode;
    list<CUI*> m_lUIObjectList;
};

NS_END
