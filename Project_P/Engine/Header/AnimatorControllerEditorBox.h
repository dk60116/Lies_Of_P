#pragma once

#include "EditorBox.h"
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <array>

NS_BEGIN(Engine)

class ENGINE_DLL CAnimatorControllerEditorBox final : public CEditorBox
{
    friend class CEditor;

public:
    enum class ESelectType { None, Param, State, Transition };
    enum class EDeleteType { None, Param, State };
    enum class EPendingSource { None, State, AnyState, Entry };

private:
    struct MotionItem
    {
        string label;
        string value;
    };

protected:
    explicit CAnimatorControllerEditorBox();
    ~CAnimatorControllerEditorBox();

private:
    static CAnimatorControllerEditorBox* Create();

public:
    void Render() override;
    void OnDestroy() override;

public:
    void Open(const fs::path& path);

private:
    struct Param
    {
        string type;   
        string name;
        string value;
    };

    struct State
    {
        string name;
        string motion;
        _float speedMul = 1.f;
        ImVec2 pos = ImVec2(100, 100);
    };

    struct Transition
    {
        string from;
        string to;
        _float blend = 0.15f;
        _bool hasExitTime = false;
        _float exitTime = 1.f;
        _bool fixedDuration = false;
        _float transitionDuration = 0.15f;
        _float transitionOffset = 0.f;
        string cond;
        _bool isAny = false;
    };

private:
    _bool LoadFromFile(const fs::path& path);
    _bool ParseText(const string& text);
    string SerializeText() const;
    _bool SaveToFile();

private:
    void RenderToolbar();
    void RenderLeftPanel();
    void RenderInspector();
    void RenderGraph();

    static string Trim(const string& s);
    static _bool StartsWith(const string& s, const char* prefix);
    static vector<string> Split(const string& s, char delim);
    static _bool TryParseVec2(const string& s, ImVec2& out);

    void RequestDeleteParam(int idx);
    void RequestDeleteState(const string& name);
    void RenderDeleteConfirmPopup();

    void DeleteParam(int idx);
    void DeleteState(const string& name);
    void CleanupTransitionsForDeletedState(const string& name);

private:
    void RequestAddParam();
    void RequestAddState();
    void RenderAddCreatePopups();
    void RenderAddParamPopup();
    void RenderAddStatePopup();

    void AddParam(const string& type, const string& name, const string& value);
    void AddState(const string& name, const string& motion, _float speedMul);

    _bool ParamNameExists(const string& name) const;
    string MakeUniqueStateName(const string& base) const;
    static string SanitizeIdentifier(const string& s);

private:
    void EnsureMotionOptionsLoaded();
    void RefreshMotionOptions();

    static string ExtractKeyBeforeColon(const string& line);
    static _bool IsSceneCommentLine(const string& trimmedLine);
    static _bool ContainsAnimationClipTag(const string& trimmedLine);

private:
    void BindParamRenameBuffer(int idx);
    void BindStateRenameBuffer(const std::string& stateName);
    
    _bool ParamNameExistsExcept(const string& name, int exceptIdx) const;
    _bool RenameParam(int idx, const string& newName, string* outError = nullptr);
    _bool RenameState(const string& oldName, const string& newName, string* outError = nullptr);

private:
    bool TransitionExists(const string& from, const string& to, _bool isAny) const;
    void AddTransition(const string& from, const string& to);
    void AddAnyTransition(const string& to);
    void DeleteTransition(_int index);

private:
    fs::path m_path;
    _bool m_bOpen, m_bLoaded;

    string m_controllerName;
    string m_entryState;

    vector<Param> m_params;
    unordered_map<string, State> m_states;
    vector<Transition> m_transitions;
    vector<Transition> m_entryTransitions;

    string m_selectedState;

    ImVec2 m_pan = ImVec2(0, 0);
    _float m_zoom = 1.f;
    ImVec2 m_anyStatePos = ImVec2(10.f, 10.f);
    ImVec2 m_entryPos = ImVec2(10.f, 60.f);

private:
    ESelectType m_eSelectType;
    _int m_iSelectedParamIndex;
    _int m_iSelectedTransitionIndex = -1;

    EDeleteType m_eDeleteType;
    _int m_iDeleteParamIndex;
    string m_strDeleteStateName;
    _bool m_bRequestDeletePopup;

private:
    _bool m_bRequestAddParamPopup = false;
    _bool m_bRequestAddStatePopup = false;

    _int  m_iNewParamType = 0; 
    _int  m_iNewParamBool = 0;
    array<char, 128> m_newParamName{};
    array<char, 128> m_newParamValue{};

    array<char, 128> m_newStateName{};
    array<char, 256> m_newStateMotion{};
    _float m_newStateSpeedMul = 1.f;
    _int m_iStateSpawnIndex = 0;

    string m_strCreateError;

private:
    vector<string> m_motionOptions;
    _bool m_bMotionOptionsDirty = true;

private:
    array<char, 128> m_editParamNameBuf{};
    array<char, 128> m_editStateNameBuf{};
    _int m_boundParamIndex = -1;
    string m_boundStateName;
    string m_strRenameError;

private:
    string m_pendingTransitionFrom;
    EPendingSource m_pendingSourceType = EPendingSource::None;
};

NS_END
