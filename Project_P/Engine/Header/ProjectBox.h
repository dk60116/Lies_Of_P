#pragma once

#include "EditorBox.h"

NS_BEGIN(Engine)

class ENGINE_DLL CProjectBox final : public CEditorBox
{
	friend class CEditor;

protected:
	explicit CProjectBox();
	~CProjectBox();

private:
	static CProjectBox* Create();

public:
	void Render() override;
	void OnDestroy() override;

private:
	void RenderAssetFoldersHierarchy();
	void RenderBinaryFoldersHierarchy();
	void RenderDirectoryRecursive(const fs::path& _dirPath);

private:
	fs::path m_strCurrentSelectedFilePath;
	fs::path m_strPendingDeletePath;
	_bool m_bRequestDelete;
};

NS_END