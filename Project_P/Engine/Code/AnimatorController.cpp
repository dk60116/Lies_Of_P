#include "epch.h"
#include "AnimatorController.h"
#include "Animator.h"

NS_BEGIN(Engine)

CAnimatorController::CAnimatorController()
{
	m_strName = L"AnimatorController";
}

CAnimatorController::~CAnimatorController()
{
}

CAnimatorController* CAnimatorController::Create()
{
	return new CAnimatorController();
}

HRESULT CAnimatorController::Initialize(const wstring& _name, const wstring& _filePath, void* _desc)
{
	if (FAILED(__super::Initialize(_name, _filePath, _desc)))
		return E_FAIL;

	return S_OK;
}

void CAnimatorController::OnDestroy()
{
	m_mParams.clear();
	m_mStates.clear();
	m_vAnyStateTransitions.clear();

	__super::OnDestroy();
}

HRESULT CAnimatorController::Initiailize_Custom(const AnimatorControllerInitInfo& _info, void* _desc)
{
	m_strResourceName = _info.controllerName;
	m_strEntryState = _info.entryState;

	m_mParams.clear();
	m_mStates.clear();
	m_vAnyStateTransitions = _info.anyStateTransitions;

	for (const auto& p : _info.parameters)
	{
		if (p.name.empty())
			continue;

		m_mParams[p.name] = p;
	}

	for (const auto& s : _info.states)
	{
		if (s.name.empty())
			continue;

		m_mStates[s.name] = s;
	}

	if (!m_strEntryState.empty())
	{
		if (m_mStates.find(m_strEntryState) == m_mStates.end())
		{
			CDebug::LogError(L"AnimatorController entry state not found: " + m_strEntryState);
		}
	}

	return S_OK;
}

const CAnimatorController::State* CAnimatorController::Find_State(const wstring& _stateName) const
{
	auto it = m_mStates.find(_stateName);
	if (it == m_mStates.end())
		return nullptr;
	return &it->second;
}

const CAnimatorController::ParameterDesc* CAnimatorController::Find_Parameter(const wstring& _paramName) const
{
	auto it = m_mParams.find(_paramName);
	if (it == m_mParams.end())
		return nullptr;
	return &it->second;
}

CAnimatorControllerInstance::CAnimatorControllerInstance()
	: m_pController(nullptr)
	, m_strCurrentState(L"")
{
}

CAnimatorControllerInstance::~CAnimatorControllerInstance()
{
	OnDestroy();
}

HRESULT CAnimatorControllerInstance::Initialize(CAnimatorController* _controller, CAnimator* _animator, const _bool _playEntry)
{
	if (!_controller || !_animator)
		return E_FAIL;

	OnDestroy();

	m_pController = _controller;
	m_pController->AddRef();

	// 런타임 파라미터 초기화(디폴트 적용)
	for (const auto& kv : m_pController->Get_ParamMap())
	{
		const auto& desc = kv.second;

		ParamValue v;
		v.type = desc.type;
		v.b = desc.defaultBool;
		v.i = desc.defaultInt;
		v.f = desc.defaultFloat;
		v.trigger = false;

		m_mRuntimeParams[desc.name] = v;
	}

	// entry state 세팅
	m_strCurrentState = m_pController->Get_EntryState();

	if (_playEntry && !m_strCurrentState.empty())
	{
		const auto* st = m_pController->Find_State(m_strCurrentState);
		if (st && !st->motionName.empty())
		{
			_animator->Set_PlaybackSpeed(st->speedMul);
			_animator->Play(st->motionName, 0.f);
		}
	}

	return S_OK;
}

void CAnimatorControllerInstance::OnDestroy()
{
	Safe_Release(m_pController);
	m_mRuntimeParams.clear();
	m_strCurrentState.clear();
}

void CAnimatorControllerInstance::SetBool(const wstring& _name, const _bool _v)
{
	auto it = m_mRuntimeParams.find(_name);
	if (it == m_mRuntimeParams.end()) return;
	if (it->second.type != CAnimatorController::PARAM_TYPE::BOOL) return;
	it->second.b = _v;
}

void CAnimatorControllerInstance::SetInt(const wstring& _name, const _int _v)
{
	auto it = m_mRuntimeParams.find(_name);
	if (it == m_mRuntimeParams.end()) return;
	if (it->second.type != CAnimatorController::PARAM_TYPE::INT) return;
	it->second.i = _v;
}

void CAnimatorControllerInstance::SetFloat(const wstring& _name, const _float _v)
{
	auto it = m_mRuntimeParams.find(_name);
	if (it == m_mRuntimeParams.end()) return;
	if (it->second.type != CAnimatorController::PARAM_TYPE::FLOAT) return;
	it->second.f = _v;
}

void CAnimatorControllerInstance::SetTrigger(const wstring& _name)
{
	auto it = m_mRuntimeParams.find(_name);
	if (it == m_mRuntimeParams.end()) return;
	if (it->second.type != CAnimatorController::PARAM_TYPE::TRIGGER) return;
	it->second.trigger = true;
}

void CAnimatorControllerInstance::ResetTrigger(const wstring& _name)
{
	auto it = m_mRuntimeParams.find(_name);
	if (it == m_mRuntimeParams.end()) return;
	if (it->second.type != CAnimatorController::PARAM_TYPE::TRIGGER) return;
	it->second.trigger = false;
}

_bool CAnimatorControllerInstance::Evaluate_Condition(const CAnimatorController::Condition& _c) const
{
	auto it = m_mRuntimeParams.find(_c.paramName);
	if (it == m_mRuntimeParams.end())
		return false;

	const ParamValue& pv = it->second;

	using PT = CAnimatorController::PARAM_TYPE;
	using OP = CAnimatorController::COMPARE_OP;

	switch (pv.type)
	{
	case PT::TRIGGER:
		// 트리거는 "발동됐는가"만 판단 (op 무시)
		return pv.trigger;

	case PT::BOOL:
		switch (_c.op)
		{
		case OP::EQUAL:     return pv.b == _c.b;
		case OP::NOT_EQUAL: return pv.b != _c.b;
		default:            return false; // bool에는 대소 비교 의미 없음
		}

	case PT::INT:
		switch (_c.op)
		{
		case OP::EQUAL:         return pv.i == _c.i;
		case OP::NOT_EQUAL:     return pv.i != _c.i;
		case OP::GREATER:       return pv.i > _c.i;
		case OP::GREATER_EQUAL: return pv.i >= _c.i;
		case OP::LESS:          return pv.i < _c.i;
		case OP::LESS_EQUAL:    return pv.i <= _c.i;
		default:                return false;
		}

	case PT::FLOAT:
		switch (_c.op)
		{
		case OP::EQUAL:         return pv.f == _c.f;
		case OP::NOT_EQUAL:     return pv.f != _c.f;
		case OP::GREATER:       return pv.f > _c.f;
		case OP::GREATER_EQUAL: return pv.f >= _c.f;
		case OP::LESS:          return pv.f < _c.f;
		case OP::LESS_EQUAL:    return pv.f <= _c.f;
		default:                return false;
		}
	}

	return false;
}

_bool CAnimatorControllerInstance::Evaluate_Transition(const CAnimatorController::Transition& _tr, CAnimator* _animator) const
{
	// ExitTime 검사
	if (_tr.hasExitTime)
	{
		// normalizeTime은 0~1 (루프면 mod 처리된 값)이라고 가정
		const _float nt = _animator->Get_StateInfo().normalizeTime;
		if (nt < _tr.exitTimeNormalized)
			return false;
	}

	// 조건 리스트 모두 true여야 통과(AND)
	for (const auto& c : _tr.conditions)
	{
		if (!Evaluate_Condition(c))
			return false;
	}

	return true;
}

void CAnimatorControllerInstance::Consume_TriggersUsedBy(const CAnimatorController::Transition& _tr)
{
	for (const auto& c : _tr.conditions)
	{
		auto it = m_mRuntimeParams.find(c.paramName);
		if (it == m_mRuntimeParams.end()) continue;

		if (it->second.type == CAnimatorController::PARAM_TYPE::TRIGGER)
			it->second.trigger = false;
	}
}

void CAnimatorControllerInstance::Update(CAnimator* _animator, const _float _dt)
{
	if (!m_pController || !_animator)
		return;

	// 현재 상태 없으면 entry로 강제
	if (m_strCurrentState.empty())
		m_strCurrentState = m_pController->Get_EntryState();

	const auto* curState = m_pController->Find_State(m_strCurrentState);
	if (!curState)
		return;

	// 1) AnyState 전이 우선 평가(유니티 느낌)
	for (const auto& tr : m_pController->Get_AnyStateTransitions())
	{
		if (tr.toState.empty()) continue;
		if (!Evaluate_Transition(tr, _animator)) continue;

		const auto* nextState = m_pController->Find_State(tr.toState);
		if (!nextState) continue;

		Consume_TriggersUsedBy(tr);

		m_strCurrentState = nextState->name;

		_animator->Set_PlaybackSpeed(nextState->speedMul);
		_animator->Play(nextState->motionName, tr.blendDuration);
		return;
	}

	// 2) 현재 상태 전이 평가
	for (const auto& tr : curState->transitions)
	{
		if (tr.toState.empty()) continue;
		if (!Evaluate_Transition(tr, _animator)) continue;

		const auto* nextState = m_pController->Find_State(tr.toState);
		if (!nextState) continue;

		Consume_TriggersUsedBy(tr);

		m_strCurrentState = nextState->name;

		_animator->Set_PlaybackSpeed(nextState->speedMul);
		_animator->Play(nextState->motionName, tr.blendDuration);
		return;
	}
}

NS_END
