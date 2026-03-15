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

	CResources& resources = CResources::GetInstance();
	const wstring orbitronFontAssetPath = L"../Assets/Fonts/orbitron-medium.otf";
	wstring orbitronSpriteFontPath = L"BinaryAssets/FontData/orbitron-medium.spritefont";
	const wstring orbitronFontResourceName = L"Orbitron Medium (Font)";

	if (!CResources::FileExists(orbitronSpriteFontPath))
	{
		if (FAILED(resources.ConvertOTFTTFToSpriteFont(orbitronFontAssetPath)))
			return E_FAIL;
	}

	CFont* orbitronFont = nullptr;
	auto fontIter = resources.m_mGameResourceList.find(orbitronFontResourceName);
	if (fontIter != resources.m_mGameResourceList.end())
		orbitronFont = dynamic_cast<CFont*>(fontIter->second);

	if (!orbitronFont)
	{
		orbitronFont = resources.CreateGameResource<CFont>(orbitronFontResourceName, L"", &orbitronSpriteFontPath);
		if (!orbitronFont)
			return E_FAIL;
	}

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

	ImageObject->GetTransform()->SetParent(canvasObj->GetTransform());

	ImageObject2->GetTransform()->SetParent(image->GetTransform());

	CGameObject* gameStartTextObject = Add_GameObject(L"Game Start Text");
	CText* gameStartText = gameStartTextObject->AddComponent<CText>();
	gameStartText->GetTransform()->SetParent(canvasObj->GetTransform());
	gameStartText->SetFont(orbitronFont);
	gameStartText->SetFontSize(6.f);
	gameStartText->SetColor(ColorValue::white());
	gameStartText->SetText(L"Start Game");

	CGameObject* exitTextObject = Add_GameObject(L"Exit Text");
	CText* exitText = exitTextObject->AddComponent<CText>();
	exitText->GetTransform()->SetParent(canvasObj->GetTransform());
	exitText->SetFont(orbitronFont);
	exitText->SetFontSize(5.f);
	exitText->SetColor(ColorValue::white());
	exitText->SetText(L"Exit");

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
