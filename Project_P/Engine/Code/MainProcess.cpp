#include "epch.h"
#include "MainProcess.h"
#include "JobSystem.h"

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
    if (FAILED(CPhysics::GetInstance().Initialize()))
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
    CPhysics::GetInstance().SetFixedDeltaTime(CSceneManager::GetInstance().Get_TimeSetting().fixedTimeStep);

    CGraphicDevice& graphicDev = CGraphicDevice::GetInstance();

	if (scene)
	{
		CSceneManager& sceneManager = CSceneManager::GetInstance();
		CEditor& editor = CEditor::GetInstance();
		const _bool hideEditorWhilePlaying = sceneManager.IsPlayMode() && editor.IsHideEditorWhilePlaying();

		scene->CollectCompletedRenderFrames();
		if (!hideEditorWhilePlaying)
			scene->Update_Editor();

		const _bool runSimulationFrame = sceneManager.IsPlaying() || sceneManager.ConsumeStepFrameRequest();
		if (runSimulationFrame)
		{
			CPhysics::GetInstance().Tick(DELTA_TIME);
			scene->FixedUpdate();
			scene->Update();
			scene->LateUpdate();
		}
		else
		{
            if (!hideEditorWhilePlaying)
			    scene->LateUpdate_Editor();
		}

		scene->EndFrame();

		const _uint renderFrameIndex = scene->PrepareRender();

		CRenderThread::GetInstance().Submit([scene, &graphicDev, renderFrameIndex]
		{
			scene->Render_Game(renderFrameIndex);
			graphicDev.Present();
			scene->CompleteRenderFrame(renderFrameIndex);
		});

#ifndef _CLIENT_BUILD
		CRenderThread::GetInstance().WaitIdle();
		scene->CollectCompletedRenderFrames();
		if (!sceneManager.IsPlayMode() && editor.IsHideEditorWhilePlaying())
			editor.SetHideEditorWhilePlaying(false);

		graphicDev.Set_RenderTarget(editor.Get_EditorWindow());
		editor.Editor_Update_Begin();
		editor.Editor_Update_During();
		editor.Editor_Update_During();

		if (sceneManager.IsPlayMode() && editor.IsHideEditorWhilePlaying())
		{
			const ColorValue clearColor = ColorValue::black();
			graphicDev.Clear_BackBuffer_View(&clearColor);
			graphicDev.Clear_DepthStencil_View();
		}
		else
		{
			scene->Render_Editor();
		}

		editor.Editor_Update_End();
		graphicDev.Present();
#endif
    }

    if (CSceneManager::GetInstance().Is_Loading() && !CSceneLoader::GetInstance().Is_Loading())
        CSceneManager::GetInstance().LoadComplete();
}

void CMainProcess::Release_MainApp()
{
    CRenderThread::GetInstance().WaitIdle();
	if (CScene* scene = CSceneManager::GetInstance().Get_CrtScene())
		scene->CollectCompletedRenderFrames();

    // Tear scenes down while dependent runtime systems are still alive.
    CSceneManager::GetInstance().Release();
    CJobSystem::GetInstance().Shutdown();
    CRenderThread::GetInstance().Shutdown();
    CUIManager::GetInstance().Release();
    CPhysics::GetInstance().Release();
    CInput::GetInstance().Release();
    CRenderTargetManager::GetInstance().Release();
    CDebug::GetInstance().Release();
    CTime::GetInstance().Release();
}
