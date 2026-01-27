#include "cpch.h"
#include "LoadingScene.h"

CLoadingScene::CLoadingScene()
	: m_pMainCamera(nullptr)
	, m_pCanvas(nullptr)
	, m_pLogoImage(nullptr)
	, m_fLoadGauge(0.f)
{
}

CLoadingScene::~CLoadingScene()
{
}

HRESULT CLoadingScene::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	CGameObject* cameraObject = Add_GameObject(L"Main Camera");
	m_pMainCamera = cameraObject->AddComponent<CCamera>();

	CGameObject* canvasObj = Add_GameObject(L"Canvas");
	m_pCanvas = canvasObj->AddComponent<CCanvas>();

	CGameObject* bgObj = Add_GameObject(L"BG");
	CImage* bgImage = bgObj->AddComponent<CImage>();
	bgImage->Get_Transform()->SetParent(m_pCanvas->Get_Transform());

	CGameObject* bottomObject = Add_GameObject(L"Bottom");
	CImage* bottomImage = bottomObject->AddComponent<CImage>();
	bottomImage->Get_Transform()->SetParent(m_pCanvas->Get_Transform());
	bottomImage->SetColor(ColorValue::black());

	CGameObject* backLogoObj = Add_GameObject(L"Logo_Back");
	CImage* backLogoImage = backLogoObj->AddComponent<CImage>();
	backLogoImage->Get_Transform()->SetParent(m_pCanvas->Get_Transform());
	backLogoImage->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"Logo (Texture)"));
	backLogoImage->SetColor(ColorValue::gray(0.5f));

	CGameObject* logoObj = Add_GameObject(L"Logo");
	m_pLogoImage = logoObj->AddComponent<CImage>();
	m_pLogoImage->Get_Transform()->SetParent(backLogoImage->Get_Transform());
	m_pLogoImage->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"Logo (Texture)"));
	m_pLogoImage->Set_FillMethod(CImage::FillMethod::Horizontal);

	return S_OK;
}

void CLoadingScene::Awake()
{
	const wstring next = CGameManager::GetInstance().Get_NextScene();

	m_fLoadGauge = 0.f;

	if (next != L"")
	{
		CSceneManager::GetInstance().LoadScene(CGameManager::GetInstance().Get_NextScene());
		CGameManager::GetInstance().Set_NexScene(L"");
	}
}

void CLoadingScene::Update()
{
	__super::Update();

	m_fLoadGauge = CSceneLoader::GetInstance().Get_LoadingProgress();

	m_fLoadGauge = std::clamp(m_fLoadGauge, 0.f, 1.f);

	m_pLogoImage->SetFillAmount(m_fLoadGauge);
}