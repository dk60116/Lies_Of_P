#include "cpch.h"
#include "MainScene.h"
#include "Player.h"

CMainScene::CMainScene()
	: CScene{}
	, m_pMainCamera(nullptr)
	, m_pCanvas(nullptr)
	, m_pLogoImage(nullptr)
{
}

CMainScene::~CMainScene()
{
}

HRESULT CMainScene::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	CGameObject* cameraObject = Add_GameObject(L"Main Camera");
	m_pMainCamera = cameraObject->AddComponent<CCamera>();

	CGameObject* canvasObj = Add_GameObject(L"Canvas");
	m_pCanvas = canvasObj->AddComponent<CCanvas>();

	CGameObject* ImageObject = Add_GameObject(L"Background");
	CImage* image = ImageObject->AddComponent<CImage>();

	image->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"Main_BG (Texture)"));

	CGameObject* ImageObject2 = Add_GameObject(L"Logo");
	m_pLogoImage = ImageObject2->AddComponent<CImage>();
	m_pLogoImage->SetTexture(CResources::GetInstance().LoadOnScene<CTexture>(L"Main_Logo (Texture)"));

	m_pLogoImage->SetFillAmount(1.f);

	ImageObject->Get_Transform()->SetParent(canvasObj->Get_Transform());

	ImageObject2->Get_Transform()->SetParent(image->Get_Transform());

	CGameObject* gameStartTextObject = Add_GameObject(L"Game Start Text");
	CText* gameStartText = gameStartTextObject->AddComponent<CText>();
	gameStartText->Get_Transform()->SetParent(canvasObj->Get_Transform());
	gameStartText->Set_FontSize(6.f);
	gameStartText->SetColor(ColorValue::white());
	gameStartText->Set_Text(L"Start Game");

	CGameObject* exitTextObject = Add_GameObject(L"Exit Text");
	CText* exitText = exitTextObject->AddComponent<CText>();
	exitText->Get_Transform()->SetParent(canvasObj->Get_Transform());
	exitText->Set_FontSize(5.f);
	exitText->SetColor(ColorValue::white());
	exitText->Set_Text(L"Exit");

	return S_OK;
}

void CMainScene::Update()
{
	__super::Update();

	if (CInput::GetInstance().GetKeyDown(Alpha1))
	{
		CGameManager::GetInstance().Set_NexScene(L"Game Scene");
		CSceneManager::GetInstance().LoadScene(L"Loading Scene");
	}
}
