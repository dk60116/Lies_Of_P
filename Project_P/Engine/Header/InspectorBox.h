#pragma once

#include "EditorBox.h"
#include <filesystem>

NS_BEGIN(Engine)

namespace fs = std::filesystem;

class ENGINE_DLL CInspectorBox final : public CEditorBox
{
	friend class CEditor;

protected:
	explicit CInspectorBox();
	~CInspectorBox();

private:
	static CInspectorBox* Create();

public:
	void Render() override;
	void OnDestroy() override;

private:
	void ShowTransform(CGameObject* _obj);
	void ShowRectTransform(CGameObject* _obj);
	void ShowComponents(CGameObject* _obj);
	void ShowAddComponentMenu(CGameObject* _obj);
	void RenderMeshRendererComponent(CMeshRenderer* _meshRenderer);
	void RenderMeshFilterComponent(CGameObject* _obj, CMeshFilter* _meshFilter);
	void RenderRigidBodyComponent(CRigidBody* _rigidBody);
	void RenderSelectedAssetInfo(const fs::path& path);
	void RenderSelectedAssetPreview(const fs::path& path);
	_float m_fRXDrag, m_fRYDrag, m_fRZDrag;
	class CTexture* m_pPreviewTexture;
	wstring m_previewAssetPath;
};

NS_END
