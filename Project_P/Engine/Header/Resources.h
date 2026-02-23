#pragma once

#include "epch.h"
#include "Scene.h"
#include "Shader.h"
#include "Texture.h"
#include "SkinnedMeshBuffer.h"
#include "AnimationClip.h"
#include "AnimatorController.h"

#include <filesystem>
namespace fs = filesystem;

NS_BEGIN(Engine)

class ENGINE_DLL CResources final
{
    SINGLETONCLASS(CResources);

    friend class CAnimator;

public:
    HRESULT Initialize();

private:
    void Release();

public:
    template<typename T>
    static T* LoadResourceComplete_Game(const wstring& _name, const wstring& _path, void* _desc = nullptr);
    template<typename T>
    static T* LoadResourceComplete_Scene(const wstring& _name, const wstring& _path, void* _desc, const bool _tempScene);

public:
    HRESULT ConvertFBXToMeshBufferData(const wstring _filePath);
    HRESULT ConvertImageToDDS(const wstring _filePath);
    HRESULT ConvertFBXToSkinnedBufferData(const wstring _filePath);
    HRESULT ConvertFBXToAnimationClipData(const wstring _filePath);
    HRESULT ConvertAnimatorControllerToBinary(const wstring _filePath);

    HRESULT ConvertOTFTTFToSpriteFont(const wstring _filePath);

public:
    HRESULT SaveSceneObjectTransformInfos(const wstring _filePath, vector<CScene::ObjectsTransformInfo> _infoList);
    vector<CScene::ObjectsTransformInfo> ReadSceneObjectTransformInfos(const wstring _binFileName);

    HRESULT SaveMeshBufferInfos(const wstring _filePath, vector<CMeshBuffer::MeshBufferInitiaizeInfo> _infoList);
    vector<CMeshBuffer::MeshBufferInitiaizeInfo> ReadMeshBufferInfos(const wstring _binFileName);
    HRESULT SaveSkinnedBufferInfos(const wstring _filePath, vector<CSkinnedMeshBuffer::SkinnedBufferInitiaizeInfo> _infoList, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _skeletonInfo);
    CSkinnedMeshBuffer::SkinnedBuffer ReadSkinnedBufferInfos(const wstring _binFileName);
    HRESULT SaveAnimationClipBufferInfos(const wstring _filePath, vector<CAnimationClip::AnimationClipInitInfo> _infoList);
    vector<CAnimationClip::AnimationClipInitInfo> ReadAnimationClipBufferInfos(const wstring _binFileName);
    CAnimatorController::AnimatorControllerInitInfo ReadAnimatorControllerBufferInfos(const wstring _binFileName);

    static CEngineResource* AddSceneResource(const wstring& _name, CEngineResource* _resource, const _bool _tempScene);
public:
    template<typename T>
    T* CreateGameResource(const wstring& _name, const wstring& _path, void* _desc = nullptr);

    template<typename T>
    T* CreateSceneResource(const wstring& _name, const wstring& _path, void* _desc = nullptr, const _bool _tempScene = false);

    vector<MeshBundle> CreateSceneMeshBundle(const wstring& _name, vector<CMeshBuffer::MeshBufferInitiaizeInfo> _infoList, _int _filter, void* _desc = nullptr, const _bool _tempScene = false);
    vector<SkinnedMeshBundle> CreateSceneSkinnedBundle(const wstring& _name, vector<CSkinnedMeshBuffer::SkinnedBufferInitiaizeInfo> _infoList, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _skelList, _int _filter, void* _desc = nullptr, const _bool _tempScene = false);

    template<typename T>
    T* LoadOnGame(const wstring& _name);
    template<typename T>
    T* CloneOnGame(const wstring& _name);

    template<typename T>
    T* LoadOnScene(const wstring& _name);
    template<typename T>
    T* CloneOnScene(const wstring& _name);

    vector<MeshBundle> LoadMeshBuffersOnScene(const wstring& _name);
    vector<SkinnedMeshBundle> LoadSkinnedMeshBuffersOnScene(const wstring& _name);
    vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> LoadSkinnedBonesOnScene(const wstring& _name);

    static _bool FileExists(const wstring& _path);
    static _bool FileExists(const string& _path);

    unordered_map<wstring, CEngineResource*> m_mEditorResourceList;
    unordered_map<wstring, CEngineResource*> m_mGameResourceList;

private:
    void Ready_GameResources();
    void TraverseSkeleton(aiNode* _node, _int _parentId, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL>& _outList);

private:
    wstring m_strDefaultAssetPath;
    wstring m_strEngineFilePath;
};

NS_END

template<typename T>
inline T* CResources::LoadResourceComplete_Game(const wstring& _name, const wstring& _path, void* _desc)
{
    CEngineResource* newResource = GetInstance().CreateGameResource<T>(_name, _path, _desc);

    if (newResource)
        CDebug::Log(L"Create Game resource successfully: " + newResource->Get_ResourceName());
    else
        CDebug::LogError(L"Failed create Scene resource: " + _name);

    return dynamic_cast<T*>(newResource);
}

template<typename T>
inline T* CResources::LoadResourceComplete_Scene(const wstring& _name, const wstring& _path, void* _desc, const bool _tempScene)
{
    CEngineResource* newResource = GetInstance().CreateSceneResource<T>(_name, _path, _desc, _tempScene);

    if (newResource)
        CDebug::Log(L"Create Scene resource successfully: " + newResource->Get_ResourceName());
    else
        CDebug::LogError(L"Failed create Scene resource: " + _name);

    return dynamic_cast<T*>(newResource);
}

template<typename T>
inline T* CResources::CreateGameResource(const wstring& _name, const wstring& _path, void* _desc)
{
    T* newResource = T::Create();

    if (!newResource)
        return nullptr;

    wstring fullPath = _path;
    if (_path.rfind(L"BinaryAssets/", 0) != 0 && _path.rfind(L"../BinaryAssets/", 0) != 0)
        fullPath = m_strDefaultAssetPath + _path;

    if (FAILED(newResource->Initialize(_name, fullPath, _desc)))
    {
        delete newResource;
        return nullptr;
    }

    m_mGameResourceList.emplace(_name, newResource);
    newResource->AddRef();

    return newResource;
}

template<typename T>
inline T* CResources::CreateSceneResource(const wstring& _name, const wstring& _path, void* _desc, const _bool _tempScene)
{
    T* newResource = T::Create();

    wstring fullPath = _path;
    if (_path.rfind(L"BinaryAssets/", 0) != 0 && _path.rfind(L"../BinaryAssets/", 0) != 0)
        fullPath = m_strDefaultAssetPath + _path;

    if (FAILED(newResource->Initialize(_name, fullPath, _desc)))
    {
        delete newResource;
        return nullptr;
    }

    CScene* targetScene = _tempScene ? CSceneManager::GetInstance().Get_TempScene() :
        CSceneManager::GetInstance().Get_CrtScene();

    if (!_tempScene)
        targetScene->Add_Resource(_name, newResource);
    else
        targetScene->Add_TempResource(_name, newResource);

    return newResource;
}

template<typename T>
inline T* CResources::LoadOnGame(const wstring& _name)
{
    auto iter = m_mGameResourceList.find(_name);

    if (iter == m_mGameResourceList.end())
    {
        CDebug::LogError(L"Failed LoadOnGame: " + _name);
        return nullptr;
    }

    T* resultResource = dynamic_cast<T*>(iter->second);

    return resultResource;
}

template<typename T>
inline T* CResources::CloneOnGame(const wstring& _name)
{
    T* proto = LoadOnGame<T>(_name);

    T* clone = T::Clone(*proto);

    if (CSceneManager::GetInstance().Get_CrtScene())
        CSceneManager::GetInstance().Get_CrtScene()->Add_CloneResourece(clone);

    if (!clone)
        CDebug::LogError(L"Failed CloneOnGame: " + _name);

    return clone;
}

template<typename T>
inline T* CResources::LoadOnScene(const wstring& _name)
{
    CEngineResource* r = nullptr;

    if (CSceneManager::GetInstance().Get_CrtScene())
        r = CSceneManager::GetInstance().Get_CrtScene()->Find_Resource(_name);

    T* resultResource = dynamic_cast<T*>(r);

    if (!r)
    {
        if (!CSceneManager::GetInstance().Get_TempScene())
        {
            CDebug::LogError(L"Failed LoadOnScene: " + _name);
            return nullptr;
        }

        CEngineResource* r = CSceneManager::GetInstance().Get_TempScene()->Find_Resource(_name);

        resultResource = dynamic_cast<T*>(r);
    }

    return resultResource;
}

template<typename T>
inline T* CResources::CloneOnScene(const wstring& _name)
{
    T* proto = LoadOnScene<T>(_name);

    if (!proto)
    {
        CDebug::LogError(L"Failed clone Scene resource - not found resource: " + _name);
        return nullptr;
    }

    T* clone = T::Clone(*proto);

    if (CSceneManager::GetInstance().Get_CrtScene())
        CSceneManager::GetInstance().Get_CrtScene()->Add_CloneResourece(clone);

    if (!clone)
        CDebug::LogError(L"Failed CloneOnScene: " + _name);

    return clone;
}
