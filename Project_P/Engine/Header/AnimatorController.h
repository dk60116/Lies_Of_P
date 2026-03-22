#pragma once

#include "EngineResource.h"

#include <unordered_map>
#include <vector>
#include <string>

NS_BEGIN(Engine)

class ENGINE_DLL CAnimatorController final : public CEngineResource
{
	friend class CResources;
	friend class CAnimator;

public:
	enum class PARAM_TYPE : _uint
	{
		BOOL,
		INT,
		FLOAT,
		TRIGGER,
	};

	enum class COMPARE_OP : _uint
	{
		EQUAL,
		NOT_EQUAL,
		GREATER,
		GREATER_EQUAL,
		LESS,
		LESS_EQUAL,
	};

	enum class STATE_MOTION_TYPE : _uint
	{
		CLIP,
		BLEND_TREE,
	};

	enum class BLEND_TREE_TYPE : _uint
	{
		ONE_D,
		TWO_D,
		DIRECT,
	};

	struct ParameterDesc
	{
		wstring     name = L"";
		PARAM_TYPE  type = PARAM_TYPE::BOOL;

		_bool   defaultBool = false;
		_int    defaultInt = 0;
		_float  defaultFloat = 0.f;
	};

	struct Condition
	{
		wstring     paramName = L"";
		COMPARE_OP  op = COMPARE_OP::EQUAL;

		_bool   b = false;
		_int    i = 0;
		_float  f = 0.f;
	};

	struct Transition
	{
		wstring toState = L"";

		_float  blendDuration = 0.15f;

		_bool   hasExitTime = false;
		_float  exitTimeNormalized = 1.f; 

		vector<Condition> conditions = {};
	};

	struct State
	{
		wstring name = L"";
		wstring motionName = L""; 

		STATE_MOTION_TYPE motionType = STATE_MOTION_TYPE::CLIP;

		_float  speedMul = 1.f;
		_float2 pos = {};

		struct BlendTreeChild
		{
			wstring motionName = L"";
			_float threshold = 0.f;
			_float2 position = {};
			wstring directParam = L"";
		};

		struct BlendTree
		{
			BLEND_TREE_TYPE type = BLEND_TREE_TYPE::ONE_D;
			wstring paramX = L"";
			wstring paramY = L"";
			_float directBlendDuration = 0.f;
			vector<BlendTreeChild> children = {};
		};

		BlendTree blendTree = {};

		vector<Transition> transitions = {};
	};

	struct AnimatorControllerInitInfo
	{
		wstring controllerName = L"";
		wstring entryState = L"";
		_float2 entryPos = {};
		_float2 anyStatePos = {};

		vector<ParameterDesc> parameters = {};
		vector<State> states = {};

		vector<Transition> anyStateTransitions = {};
		vector<Transition> entryStateTransitions = {};
	};

protected:
	CAnimatorController();
	~CAnimatorController();

protected:
	static CAnimatorController* Create();
	HRESULT Initialize(const wstring& _name, const wstring& _filePath, void* _desc) override;
	void OnDestroy() override;

public:
	HRESULT Initiailize_Custom(const AnimatorControllerInitInfo& _info);

public:
	const wstring& Get_EntryState() const { return m_strEntryState; }

	const State* Find_State(const wstring& _stateName) const;
	const ParameterDesc* Find_Parameter(const wstring& _paramName) const;

	const unordered_map<wstring, State>& Get_StateMap() const { return m_mStates; }
	const unordered_map<wstring, ParameterDesc>& Get_ParamMap() const { return m_mParams; }
	const vector<Transition>& Get_AnyStateTransitions() const { return m_vAnyStateTransitions; }
	const vector<Transition>& Get_EntryStateTransitions() const { return m_vEntryStateTransitions; }

private:
	wstring m_strEntryState = L"";

	unordered_map<wstring, ParameterDesc> m_mParams;
	unordered_map<wstring, State>         m_mStates;
	vector<Transition>                    m_vAnyStateTransitions;
	vector<Transition>                    m_vEntryStateTransitions;
};

NS_END

NS_BEGIN(Engine)

class ENGINE_DLL CAnimatorControllerInstance
{
	friend class CAnimator;

public:
	struct ParamValue
	{
		CAnimatorController::PARAM_TYPE type = CAnimatorController::PARAM_TYPE::BOOL;

		_bool  b = false;
		_int   i = 0;
		_float f = 0.f;

		_bool  trigger = false;
	};

public:
	CAnimatorControllerInstance();
	~CAnimatorControllerInstance();

	HRESULT Initialize(CAnimatorController* _controller, CAnimator* _animator, const _bool _playEntry = true);
	void OnDestroy();

	void Update(CAnimator* _animator, const _float _dt);

	void EnterEntry(CAnimator* _animator);

public:
	void SetBool(const wstring& _name, const _bool _v);
	void SetInt(const wstring& _name, const _int _v);
	void SetFloat(const wstring& _name, const _float _v);
	void SetTrigger(const wstring& _name);
	void ResetTrigger(const wstring& _name);

	const _bool GetBool(const wstring& n, _bool& out) const;
	const _bool GetInt(const wstring& n, _int& out) const;
	const _bool GetFloat(const wstring& n, _float& out) const;
	const _bool TryGetParamValue(const wstring& n, _float& out) const;

public:
	const wstring& Get_CurrentState() const { return m_strCurrentState; }

private:
	_bool Evaluate_Transition(const CAnimatorController::Transition& _tr, CAnimator* _animator) const;
	_bool Evaluate_Condition(const CAnimatorController::Condition& _c) const;

	void Consume_TriggersUsedBy(const CAnimatorController::Transition& _tr);

private:
	CAnimatorController* m_pController;
	unordered_map<wstring, ParamValue> m_mRuntimeParams;

	wstring m_strCurrentState;

	_bool m_bEntered;
};

NS_END

