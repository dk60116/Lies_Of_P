#include "cpch.h"
#include "GameScene.h"
#include "Player.h"
#include "PlayerCamera.h"
#include "PlayerHUD.h"
#include "Wolf.h"

CGameScene::CGameScene()
	: m_pPlayerCamera(nullptr)
	, m_pDirLight(nullptr)
	, m_pPlayer(nullptr)
	, m_pHUD(nullptr)
	, m_vMonsters({})
{
}

CGameScene::~CGameScene()
{
}

HRESULT CGameScene::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	CGameObject* cube = Add_GameObject(L"Cube");
	CMeshRenderer* cubeMesh = cube->AddComponent<CMeshRenderer>();
	cubeMesh->Get_MeshFilter()->Set_MeshBuffer(CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Cube (Mesh Buffer)"));

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

	CGameObject* vahMedoh_BodyObj_00 = Add_GameObject(L"Stage_01_Building_00");
	vahMedoh_BodyObj_00->CreateMeshHierachy(CResources::GetInstance().LoadMeshBuffersOnScene(L"Stage01_Bulding_00 (MeshBuffer)"), 0.01f);
	CGameObject* vahMedoh_BodyObj = Add_GameObject(L"Stage_01_Building_01");
	vahMedoh_BodyObj->CreateMeshHierachy(CResources::GetInstance().LoadMeshBuffersOnScene(L"Stage01_Bulding_01 (MeshBuffer)"), 0.01f);

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
