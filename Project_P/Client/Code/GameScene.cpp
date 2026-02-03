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

	CGameObject* vahMedoh_BodyObj = Add_GameObject(L"VahMedoh_Body");
	vahMedoh_BodyObj->CreateMeshHierachy(CResources::GetInstance().LoadMeshBuffersOnScene(L"VahMedoh (MeshBuffer)"), 0.01f);

	//CGameObject* wolfObject = Add_GameObject(L"Wolf");
	//CWolf* woolf = wolfObject->AddComponent<CWolf>();
	//m_vMonsters.push_back(woolf);

	//if (wolfObject)
	//{
	//	for (_uint i = 0; i < 2; ++i)
	//	{
	//		CGameObject* cloneWolf = CGameObject::Instantiate(wolfObject);
	//		m_vMonsters.push_back(cloneWolf->GetComponent<CWolf>());
	//	}
	//}

	//m_pPlayer->Set_Focus(wolfObject->Get_Transform());

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

	//if (CInput::GetInstance().GetKeyDown(L))
	//	m_vMonsters[2]->Get_Animator()->Play(L"Idle", 0.1f);
	//if (CInput::GetInstance().GetKeyDown(K))
	//	m_vMonsters[2]->Get_Animator()->Play(L"Run", 0.1f);
	//if (CInput::GetInstance().GetKeyDown(J))
	//	m_vMonsters[2]->Get_Animator()->Play(L"Attack01", 0.1f);
	//if (CInput::GetInstance().GetKeyDown(H))
	//	m_vMonsters[2]->Get_Animator()->Play(L"Find", 0.1f);
}

void CGameScene::SceneRelease()
{
	__super::SceneRelease();

	m_vMonsters.clear(); 
}
