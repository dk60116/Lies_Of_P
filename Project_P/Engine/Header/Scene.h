#pragma once

#include "Object.h"
#include "Renderer.h"
#include "SkinnedMeshBuffer.h"
#include "NaviMesh.h"

NS_BEGIN(Engine)

class ENGINE_DLL CScene abstract : public UObject
{
    friend class CSceneManager;
    friend class CSceneLoader;

public:
    typedef struct ObjectsRectTransfomInfo
    {
        _float2 anchoredPos = {};
        _float2 widthHeight = {};
        _float2 pivot = {};
        _float2 anchorMin = {};
        _float2 anchorMax = {};
    }SCENERECTINFO;

    typedef struct ObjectsTransformInfo
    {
        typedef struct MaterialTextureInfo
        {
            wstring name = L"";
            wstring path = L"";
        }MATERIALTEXTUREINFO;

        _uint objID = 0;
        wstring objGuid = L"";
        wstring objName = L"";
        wstring objTag = L"";
        wstring objPath = L"";
        _float3 localPos = {};
        _float4 localQuaternion = {};
        _float3 localScale = {};
        _bool isActive = true;
        _uint objLayer = 0u;
        _bool isTransformStatic = false;
        _bool isNavigationStatic = false;
        _bool rigidBodyKinematic = false;
        _bool rigidBodyUseGravity = true;
        _float rigidBodyMass = 1.f;
        _bool rigidBodyConstPositionX = false;
        _bool rigidBodyConstPositionY = false;
        _bool rigidBodyConstPositionZ = false;
        _bool rigidBodyConstRotationX = false;
        _bool rigidBodyConstRotationY = false;
        _bool rigidBodyConstRotationZ = false;
        _bool hasNaviMeshAgent = false;
        wstring navAgentNavigationMeshResourceName = L"";
        _float navAgentMoveSpeed = 3.5f;
        _float navAgentAngularSpeed = 720.f;
        _float navAgentStoppingDistance = 0.15f;
        _bool navAgentAlwaysLookAt = false;
        _float navAgentWaypointTolerance = 0.1f;
        _float navAgentRadius = 0.35f;
        _float navAgentHeight = 2.f;
        _float3 navAgentCenter = { 0.f, 1.f, 0.f };
        _float navAgentGroundSnapOffset = 0.02f;
        _bool isRect = false;
        SCENERECTINFO rectInfo = {};
        vector<wstring> componentNames = {};
        vector<_bool> componentEnabledStates = {};
        wstring meshBufferName = L"";
        wstring materialName = L"";
        vector<MATERIALTEXTUREINFO> materialTextures = {};
        vector<pair<wstring, _float>> materialFloatValues = {};
        vector<pair<wstring, _int>> materialIntValues = {};
        vector<pair<wstring, _float2>> materialVector2Values = {};
        vector<pair<wstring, _float3>> materialVector3Values = {};
        vector<pair<wstring, _float4>> materialVector4Values = {};
        vector<pair<wstring, _float4x4>> materialMatrixValues = {};
    }SCENETRANSFORMINFO;

    typedef struct SceneNavigationInfo
    {
        wstring resourceName = L"";
        EngineAI::CNaviMesh::NavBakeOptions bakeOptions = {};
    }SCENENAVIGATIONINFO;

public:
    struct EnviromentSettings
    {
        wstring skyBox = L"DefaultSky (SkyBox)";
        _float ambient = 0.2f;
        _float directionalLightShadowDist = 200.f;
        _float shadowBias = 0.f;
    };

protected:
    CScene();
    ~CScene();

public:
    virtual HRESULT Initialize();
    virtual void Awake();
    virtual void Start();
    virtual void Update_Editor();
    virtual void Update();
    virtual void FixedUpdate();
    virtual void LateUpdateEditor();
    virtual void LateUpdate();
    virtual void Render_Editor();
    virtual void Render_Game();
    virtual void SceneRelease();
    virtual void EndFrame();

protected:
    void RenderSkyBox(class CCamera* _camera);

public:
    void Set_SceneName(const wstring _name);
    const wstring& Get_SceneName() const;

public:
    vector<SCENETRANSFORMINFO> Convert_ObjectsTransformInfo() const;
    void Bind_ObjectsTransform(const vector<SCENETRANSFORMINFO> _infoList);
    vector<SCENENAVIGATIONINFO> Convert_NavigationInfos() const;
    void Bind_NavigationInfos(const vector<SCENENAVIGATIONINFO>& _infoList);

public:
    class CEngineResource* Add_Resource(const wstring& _name, CEngineResource* _resource);
    _bool Remove_Resource(const wstring& _name);
    class CEngineResource* Find_Resource(const wstring& _name);
    vector<MeshBundle> Find_MeshInfoResource(const wstring& _name);
    vector<SkinnedMeshBundle> Find_SkinnedMeshInfoResource(const wstring& _name);
    vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> Find_SkinnedBonesResource(const wstring& _name);
    class CEngineResource* Add_TempResource(const wstring& _name, CEngineResource* _resource);
    void Add_MeshBundle(const wstring& _name, vector<MeshBundle> _resource);
    void Add_SkinnedBundle(const wstring& _name, vector<SkinnedMeshBundle> _resource);
    void Add_TempMeshBundle(const wstring& _name, vector<MeshBundle> _resource);
    void Add_TempSkinnedBundle(const wstring& _name, vector<SkinnedMeshBundle> _resource);
    void Add_SkinnedMeshBone(const wstring& _name, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _resource);
    void Add_TempSkinnedMeshBone(const wstring& _name, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _resource);
    class CEngineResource* Add_CloneResourece(CEngineResource* _resource);
    class CGameObject* Add_GameObject(wstring _name);
    list<CGameObject*>& Get_ObjectList();
    vector<CGameObject*> Get_RootObjects();
    vector<CRenderer*> Get_MeshObjects();

    const EnviromentSettings& Get_EnviromentSetting();

    void Set_Ambient(const _float _value);
    void Set_DirectionalLightShadowDist(const _float _value);
    void Set_ShadwoBias(const _float _value);

    class CCamera* Get_Camera() const;
    CCamera* Get_Camera(const _int _index) const;
    CCamera* Get_EditorCamera() const;
    const list <CCamera*>& Get_CameraList();
    CCamera* Add_Camera(CCamera* _camera);
    void Remove_Camera(CCamera* _camera);

    const list<class CLight*>& Get_LightList();
    CLight* Add_Light(CLight* _light);
    void Remove_Light(CLight* _light);

    vector<_matrix>& Get_LightData();

    class CCanvas* Get_Canvas(const _int _index) const;
    const list <CCanvas*>& Get_CanvasList();
    CCanvas* Add_Canvas(CCanvas* _canvas);
    void Remove_Canvas(CCanvas* _canvas);

    HRESULT SaveScene(const wstring& _filePath);
    const _uint Get_UniqueObjectCount() const;
	const _bool Is_SaveRegistrationEnabled() const;
	void Set_SaveRegistrationEnabled(const _bool _enabled);

    CGameObject* FindGameObjectOfId(const _uint id);

public:
    ID3D11DepthStencilState* Get_MeshStencillState() const;
    ID3D11DepthStencilState* Get_UIStencillState() const;
    ID3D11DepthStencilState* Get_TransparentDepthStencillState() const;
    ID3D11BlendState* Get_BlendingState() const;
    ID3D11BlendState* Get_NoneBlendingState() const;

protected:
    HRESULT PreLoadResources();

protected:
    ID3D11Device* m_pDevice;
    ID3D11DeviceContext* m_pContext;

private:
    void PickObjectInEditor_Start();
    void PickObjectInEditor_End();

protected:
    _uint m_iSceneIndex;
    wstring m_strSceneName;
    EnviromentSettings m_sEnviromentSettings;
    class CSkyBox* m_pSkyBox;
    list <CGameObject*> m_lObjectList;
    list <CCamera*> m_lCameraList;
    list <CLight*> m_lLightList;
    list<CCanvas*> m_lCanvasList;

    CCamera* m_pEditorCamera;

    unordered_map<wstring, CEngineResource*> m_mResourceList, m_mTempResourceList;
    unordered_map<wstring, vector<MeshBundle>> m_mMeshBundleList, m_mTempMeshBundleList;
    unordered_map<wstring, vector<SkinnedMeshBundle>> m_mSkinnedBundleList, m_mTempSkinnedBundleList;
    unordered_map<wstring, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL>> m_mSkinnedBoneList, m_mTempSkinnedBoneList;
    vector<CEngineResource*> m_vCloneResourceList;

protected:
    _uint m_iUniqueObjectCount;
    unordered_map<_uint, CGameObject*> m_mObjectOfId;

    ID3D11DepthStencilState* m_pSkyBoxDepthStencillState, * m_pMeshDepthStencilState, * m_pUIDepthStencilState, * m_pTransparentDepthStencilState;
    ID3D11RasterizerState* m_pSkyBoxResterizerState, * m_pMeshResterizerState, * m_pUIResterizerState;
    ID3D11BlendState* m_pBlendingState, * m_pNoneBlendingState;

    vector<_matrix> m_vLightData;

    _float m_fPssedTime;

    vector2Int m_vTempPickMousePos;
    _bool m_bSaveRegistrationEnabled;
};

NS_END



