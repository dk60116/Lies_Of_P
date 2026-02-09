#include "cpch.h"
#include "GameScene.h"
#include "Player.h"
#include "PlayerCamera.h"
#include "PlayerHUD.h"
#include "Wolf.h"
#include "Map_01_SilentStreet.h"

CGameScene::CGameScene()
	: m_pPlayerCamera(nullptr)
	, m_pDirLight(nullptr)
	, m_pPlayer(nullptr)
	, m_pHUD(nullptr)
	, m_vMonsters({})
	, m_pMap(nullptr)
{
}

CGameScene::~CGameScene()
{
}

HRESULT CGameScene::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	CGameObject* cameraObject = Add_GameObject(L"Player Camera");
	m_pPlayerCamera = cameraObject->AddComponent<CPlayerCamera>();

	CGameObject* lightObject = Add_GameObject(L"Directional Light");
	m_pDirLight = lightObject->AddComponent<CLight>();
	m_pDirLight->Set_Intensity(0.6f);

	m_pDirLight->Get_Transform()->Set_EulerAngles(40.f, -180.f, 0.f);

	CGameObject* lightObject2 = Add_GameObject(L"Point Light");
	CLight* pointLight = lightObject2->AddComponent<CLight>();
	pointLight->Set_Type(CLight::Type::point);

	CGameObject* hudObject = Add_GameObject(L"Player HUD");
	m_pHUD = hudObject->AddComponent<CPlayerHUD>();

	CGameObject* playerObj = Add_GameObject(L"Player");
	m_pPlayer = playerObj->AddComponent<CPlayer>();

	CGameObject* mapObj = Add_GameObject(L"Map");
	mapObj->AddComponent<CMap_01_SilentStreet>();

	return S_OK;
}

void CGameScene::Awake()
{
	__super::Awake();
}

void CGameScene::Update()
{
	__super::Update();

	if (CInput::GetInstance().GetKeyDown(Alpha1))
	{
		CGameManager::GetInstance().Set_NexScene(L"Main Scene");
		CSceneManager::GetInstance().LoadScene(L"Loading Scene");
	}
}

void CGameScene::SceneRelease()
{
	__super::SceneRelease();

	m_vMonsters.clear(); 
}
