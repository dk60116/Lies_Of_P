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
	void RenderCreateAnimatorControllerPopup();
	void RenderCreateFolderPopup();

	void CreateAnimatorControllerFile(const fs::path& dir, const string& name);
	void CreateFolder(const fs::path& dir, const string& name);
	string MakeAnimatorControllerTemplateText(const string& controllerName);

private:
	fs::path m_strCurrentSelectedFilePath;
	fs::path m_strPendingDeletePath;
	_bool m_bRequestDelete;
	_bool m_bPendingDeleteIsDirectory;

	fs::path m_createTargetDir;
	_bool m_bRequestCreateAC;
	array<char, 128> m_newACName;
	fs::path m_createFolderTargetDir;
	_bool m_bRequestCreateFolder;
	array<char, 128> m_newFolderName;
	array<char, 128> m_searchBuffer{};
};

NS_END
