#include "epch.h"
#include "MainProcess.h"

CMainProcess::CMainProcess()
{
}

CMainProcess::~CMainProcess()
{
}

CMainProcess& CMainProcess::GetInstance()
{
    static CMainProcess inst;
    return inst;
}

HRESULT CMainProcess::Initialize()
{
    if (FAILED(CTime::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CDebug::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CGraphicDevice::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CRandom::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CEditor::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CResources::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CSceneManager::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CRenderTargetManager::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CSceneLoader::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CInput::GetInstance().Initialize()))
        return E_FAIL;
    if (FAILED(CUIManager::GetInstance().Initialize()))
        return E_FAIL;

#ifndef _CLIENT_BUILD
    CEditor::EDITORWINOPTION sOption = CEditor::GetInstance().Get_Options();

    vector2Int offsetMin = vector2Int(0, (_int)sOption.topBarHeight);
    vector2Int offsetMax = vector2Int(_int(sOption.projectWidth + sOption.hierachyWidth + sOption.inspectorWidth), 0);

    CGraphicDevice::GetInstance().Add_SwapChain
    (
        CEditor::GetInstance().Get_EditorWindow(),
        WINMODE::MODE_WINDOW,
        CEditor::GetInstance().Get_WindowResolution().x,
        CEditor::GetInstance().Get_WindowResolution().y,
        offsetMin,
        offsetMax
    );
#endif

    CGraphicDevice::GetInstance().Add_SwapChain
    (
        CDisplay::GetInstance().Get_GameWindow(),
        WINMODE::MODE_WINDOW,
        CDisplay::GetInstance().Get_ScreenResolution().x,
        CDisplay::GetInstance().Get_ScreenResolution().y
    );

    MSG msg;

    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return S_OK;
}

void CMainProcess::Update_MainApp()
{
    CScene* scene = CSceneManager::GetInstance().Get_CrtScene();

    CTime::GetInstance().Update();
    CInput::GetInstance().Update();

    CGraphicDevice& graphicDev = CGraphicDevice::GetInstance();

    if (scene)
    {
        scene->Update_Editor();
        scene->Update();
        scene->LateUpdate();

#ifndef _CLIENT_BUILD
        graphicDev.Set_RenderTarget(CEditor::GetInstance().Get_EditorWindow());

        CEditor::GetInstance().Editor_Update_Begin();
        CEditor::GetInstance().Editor_Update_During();
        scene->Render_Editor();
        CEditor::GetInstance().Editor_Update_End();

        graphicDev.Present();
#endif

        graphicDev.Set_RenderTarget(CDisplay::GetInstance().Get_GameWindow());
        scene->Render_Game();
        graphicDev.Present();
    }

    if (CSceneManager::GetInstance().Is_Loading() && !CSceneLoader::GetInstance().Is_Loading())
        CSceneManager::GetInstance().LoadComplete();
}

void CMainProcess::Release_MainApp()
{
    CDebug::GetInstance().Release();
}