#include "epch.h"
#include "AnimatorController.h"
#include "Animator.h"

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
    m_vEntryStateTransitions.clear();
    __super::OnDestroy();
}

HRESULT CAnimatorController::Initiailize_Custom(const AnimatorControllerInitInfo& _info)
{
    m_strResourceName = _info.controllerName;
    m_strEntryState = _info.entryState;

    m_mParams.clear();
    m_mStates.clear();
    m_vAnyStateTransitions = _info.anyStateTransitions;
    m_vEntryStateTransitions = _info.entryStateTransitions;

    for (const auto& p : _info.parameters)
    {
        if (!p.name.empty())
            m_mParams[p.name] = p;
    }

    for (const auto& s : _info.states)
    {
        if (!s.name.empty())
            m_mStates[s.name] = s;
    }

    if (!m_strEntryState.empty())
    {
        if (m_mStates.find(m_strEntryState) == m_mStates.end())
            CDebug::LogError(L"AnimatorController entry state not found: " + m_strEntryState);
    }

    return S_OK;
}

const CAnimatorController::State*
CAnimatorController::Find_State(const wstring& _stateName) const
{
    auto it = m_mStates.find(_stateName);
    return (it == m_mStates.end()) ? nullptr : &it->second;
}

const CAnimatorController::ParameterDesc*
CAnimatorController::Find_Parameter(const wstring& _paramName) const
{
    auto it = m_mParams.find(_paramName);
    return (it == m_mParams.end()) ? nullptr : &it->second;
}


// =======================
// CAnimatorControllerInstance
// =======================

static constexpr _float FLOAT_EPS = 1e-5f;

CAnimatorControllerInstance::CAnimatorControllerInstance()
    : m_pController(nullptr)
    , m_strCurrentState(L"")
    , m_bEntered(false)
{
}

CAnimatorControllerInstance::~CAnimatorControllerInstance()
{
    OnDestroy();
}

HRESULT CAnimatorControllerInstance::Initialize(
    CAnimatorController* _controller,
    CAnimator* _animator,
    const _bool _playEntry)
{
    if (!_controller || !_animator)
        return E_FAIL;

    OnDestroy();

    m_pController = _controller;
    m_pController->AddRef();

    // 런타임 파라미터 초기화
    for (const auto& kv : m_pController->Get_ParamMap())
    {
        const auto& desc = kv.second;

        ParamValue v{};
        v.type = desc.type;
        v.b = desc.defaultBool;
        v.i = desc.defaultInt;
        v.f = desc.defaultFloat;
        v.trigger = false;

        m_mRuntimeParams[desc.name] = v;
    }

    m_bEntered = false;

    if (_playEntry)
        EnterEntry(_animator);

    return S_OK;
}

void CAnimatorControllerInstance::OnDestroy()
{
    Safe_Release(m_pController);
    m_mRuntimeParams.clear();
    m_strCurrentState.clear();
    m_bEntered = false;
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

_bool CAnimatorControllerInstance::Evaluate_Condition(
    const CAnimatorController::Condition& _c) const
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
        return pv.trigger;

    case PT::BOOL:
        if (_c.op == OP::EQUAL)     return pv.b == _c.b;
        if (_c.op == OP::NOT_EQUAL) return pv.b != _c.b;
        return false;

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
        case OP::EQUAL:         return fabs(pv.f - _c.f) < FLOAT_EPS;
        case OP::NOT_EQUAL:     return fabs(pv.f - _c.f) >= FLOAT_EPS;
        case OP::GREATER:       return pv.f > _c.f;
        case OP::GREATER_EQUAL: return pv.f >= _c.f;
        case OP::LESS:          return pv.f < _c.f;
        case OP::LESS_EQUAL:    return pv.f <= _c.f;
        default:                return false;
        }
    }

    return false;
}

_bool CAnimatorControllerInstance::Evaluate_Transition(
    const CAnimatorController::Transition& _tr,
    CAnimator* _animator) const
{
    if (_tr.hasExitTime)
    {
        const _float nt = _animator->Get_StateInfo().normalizeTime;
        if (nt < _tr.exitTimeNormalized)
            return false;
    }

    for (const auto& c : _tr.conditions)
    {
        if (!Evaluate_Condition(c))
            return false;
    }

    return true;
}

void CAnimatorControllerInstance::Consume_TriggersUsedBy(
    const CAnimatorController::Transition& _tr)
{
    for (const auto& c : _tr.conditions)
    {
        auto it = m_mRuntimeParams.find(c.paramName);
        if (it == m_mRuntimeParams.end()) continue;

        if (it->second.type == CAnimatorController::PARAM_TYPE::TRIGGER)
            it->second.trigger = false;
    }
}

void CAnimatorControllerInstance::EnterEntry(CAnimator* _animator)
{
    if (m_bEntered || !_animator || !m_pController)
        return;

    // Entry Transitions 우선
    for (const auto& tr : m_pController->Get_EntryStateTransitions())
    {
        if (tr.toState.empty()) continue;
        if (!Evaluate_Transition(tr, _animator)) continue;

        const auto* st = m_pController->Find_State(tr.toState);
        if (!st) continue;

        Consume_TriggersUsedBy(tr);

        m_strCurrentState = st->name;
        _animator->Set_PlaybackSpeed(st->speedMul);
        _animator->Play(st->motionName, tr.blendDuration);

        m_bEntered = true;
        return;
    }

    // Fallback Entry State
    m_strCurrentState = m_pController->Get_EntryState();
    const auto* st = m_pController->Find_State(m_strCurrentState);
    if (st && !st->motionName.empty())
    {
        _animator->Set_PlaybackSpeed(st->speedMul);
        _animator->Play(st->motionName, 0.f);
    }

    m_bEntered = true;
}

void CAnimatorControllerInstance::Update(CAnimator* _animator, const _float _dt)
{
    if (!m_pController || !_animator)
        return;

    // Entry는 Initialize에서 이미 끝나야 함
    if (!m_bEntered)
        return;

    const auto* curState = m_pController->Find_State(m_strCurrentState);
    if (!curState)
        return;

    // 1) AnyState
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

    // 2) Current State
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