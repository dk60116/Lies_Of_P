#pragma once

#include "epch.h"

NS_BEGIN(Engine)

namespace fs = std::filesystem;

class ENGINE_DLL CEditor final
{
public:
	enum class TransformControleTool { VIEW, MOVE, ROTATE, SCALE, RECT, TRANSFORM };

	typedef struct EditopWindowOptionsDescription
	{
		_uint windowWidth = 1750;
		_uint windowHeight = 720;
		_uint topBarHeight = 28;
		_uint projectWidth = 220;
		_uint hierachyWidth = 250;
		_uint inspectorWidth = 260;

	}EDITORWINOPTION;

	SINGLETONCLASS(CEditor);

public:
	HRESULT Initialize();
	void Release();

public:
	HWND Get_EditorWindow();
	void Editor_Update_Begin();
	void Editor_Update_During();
	void Editor_Update_End();

public:
	EDITORWINOPTION Get_Options() const;
	const vector2Int Get_WindowResolution() const;
	const vector2Int Get_ScreenResolution() const;

public:
	const TransformControleTool Get_ControleTool() const;
	void Change_ControleTool(const TransformControleTool _tool);
	const vector3 Get_EditorCamPositon() const;
	const quaternion Get_EditorCamQuaternion() const;
	void Set_EditorCamTransform(class CTransform* _transform);
	void Set_SelectedGameObject(class CGameObject* _target);
	void Add_SelectedGameObject(class CGameObject* _target);
	void Remove_SelectedGameObject(class CGameObject* _target);
	void Clear_SelectedGameObjects();
	void MoveTo_SelectedGameObject(class CGameObject* _target);
	CGameObject* Get_SelectedGameObject() const;
	const vector<CGameObject*>& Get_SelectedGameObjects() const;
	bool Is_SelectedGameObject(class CGameObject* _target) const;

public:
	void OpenAsset(const fs::path& path);
	void OpenAnimatorController(const fs::path& path);

	void OpenAssetExternal(const fs::path& path);
	void Set_SelectedAssetPath(const fs::path& path);
	const fs::path& Get_SelectedAssetPath() const { return m_selectedAssetPath; }

	static string ToLowerCopy(string s)
	{
		transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c) 
			{ 
				return (char)std::tolower(c); 
			});
		return s;
	}

private:
	HWND m_hEditorWindow;

private:
	HWND CreateEditorWindow();
	static LRESULT CALLBACK EditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void ChangeControleTool();

	_bool m_bDoubleClicked;

	vector3 m_vCameraPos;
	quaternion m_vCameraQuat;
	_bool m_bIsMovingCamera;
	vector3 m_vCameraMoveStartPos;
	vector3 m_vCameraMoveTargetPos;
	_float m_fCameraMoveProgress;
	_float m_fCameraMoveDuration;

private:
	map<wstring, class CEditorBox*> m_mBoxList;
	class CAnimatorControllerEditorBox* m_pAnimatorControllerBox;

private:
	EDITORWINOPTION m_sOptions;

private:
	TransformControleTool m_eControleTool;
	CGameObject* m_pSelectedGameObject;
	CGameObject* m_pMoveTargetGameObject;
	vector<CGameObject*> m_selectedGameObjects;
	fs::path m_selectedAssetPath;

private:
	HICON m_hEditorWindowIcon_Default, m_hEditorWindoIcon_Small;
};

NS_END
