#include "cpch.h"
#include "PlayerState_Locomotion.h"

CPlayerState_Locomotion::CPlayerState_Locomotion()
	: m_pChild(nullptr)
	, m_mChildList({})
{
}

CPlayerState_Locomotion::~CPlayerState_Locomotion()
{
}

void CPlayerState_Locomotion::SetChildren(const unordered_map<CPlayerController::PlayerState, CPlayerState*> _childList)
{
	m_mChildList = _childList;
}

void CPlayerState_Locomotion::Initialize(CPlayerControllerContext* _ctx, const CPlayerController::PlayerState _type)
{
    __super::Initialize(_ctx, _type);

    {
        CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_Sprint_End (Animation Clip)");

        const wstring clipName = startClip->Get_ResourceName();
        const _uint frameCount = startClip->Get_FrameCount();

        {
            CAnimationClip::ActionTrigger at = { 1, L"SprintEndStart" };
            startClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"SprintEndStart", [this]()
                {
                    m_pCtx->StopMoveImmediate();
                });
        }
    }

    {
        CAnimationClip* startClip = CResources::GetInstance().LoadOnScene<CAnimationClip>(L"Eve_BattleSprint_End (Animation Clip)");

        const wstring clipName = startClip->Get_ResourceName();
        const _uint frameCount = startClip->Get_FrameCount();

        {
            CAnimationClip::ActionTrigger at = { 1, L"BattleSprintEndStart" };
            startClip->Add_ActionTrigger(at);
            m_pCtx->Animator()->RegisterActionHandler(L"BattleSprintEndStart", [this]()
                {
                    m_pCtx->StopMoveImmediate();
                });
        }
    }
}

void CPlayerState_Locomotion::Enter()
{
	__super::Enter();

	m_pChild = m_mChildList[CPlayerController::PlayerState::Idle];

	if (m_pChild) 
		m_pChild->Enter();
}

void CPlayerState_Locomotion::Update()
{
    __super::Update();

    // 1) 버퍼 타이머는 반복으로 Tick
    static const std::array<CPlayerController::PlayerState, 4> kTickStates =
    {
        CPlayerController::PlayerState::Attack,
        CPlayerController::PlayerState::Guard,
        CPlayerController::PlayerState::Evade,
        CPlayerController::PlayerState::Jump
    };

    for (TRAVERSAL_ITER(kTickStates, it))
        m_pCtx->TickActionBuffer(*it);

    CPlayerState* next = nullptr;
    bool handled = false;

    static const array<CPlayerController::PlayerState, 4> kBufferedPriority =
    {
        CPlayerController::PlayerState::Jump,
        CPlayerController::PlayerState::Evade,
        CPlayerController::PlayerState::Guard,
        CPlayerController::PlayerState::Attack
    };

    for (TRAVERSAL_ITER(kBufferedPriority, it))
    {
        const auto st = *it;

        if (!m_pCtx->HasActionBuffered(st))
            continue;

        if (st == CPlayerController::PlayerState::Attack &&
            m_pCtx->IsActionActive(CPlayerController::PlayerState::Guard) &&
            !m_pCtx->IsCanAttack())
        {
            m_pCtx->ConsumeActionBuffer(st);
            handled = true;
            break;
        }

        m_pCtx->ConsumeActionBuffer(st);

        auto f = m_mChildList.find(st);
        if (f != m_mChildList.end())
            next = f->second;

        handled = true;
        break;
    }

    if (!handled)
    {
        static const array<CPlayerController::PlayerState, 4> kActivePriority =
        {
            CPlayerController::PlayerState::Evade,
            CPlayerController::PlayerState::Guard,
            CPlayerController::PlayerState::Attack,
            CPlayerController::PlayerState::Jump
        };

        for (TRAVERSAL_ITER(kActivePriority, it))
        {
            const auto st = *it;

            if (!m_pCtx->IsActionActive(st))
                continue;

            auto f = m_mChildList.find(st);
            if (f != m_mChildList.end())
                next = f->second;

            handled = true;
            break;
        }
    }

    if (!handled)
    {
        const auto st = m_pCtx->IsKeyPressed_Hold(CPlayerController::PlayerState::Move)
            ? CPlayerController::PlayerState::Move
            : CPlayerController::PlayerState::Idle;

        auto f = m_mChildList.find(st);
        if (f != m_mChildList.end())
            next = f->second;
    }

    TransitionTo(next);

    if (m_pChild)
        m_pChild->Update();

    m_pCtx->TickTurn(m_pCtx->PlayerStatus().turnSpeed);
    m_pCtx->TickMove();
}

void CPlayerState_Locomotion::Exit()
{
	__super::Exit();

	if (m_pChild)
		m_pChild->Exit();

	m_pChild = nullptr;
}

void CPlayerState_Locomotion::TransitionTo(CPlayerState* _next)
{
	if (!_next || _next == m_pChild)
		return;

	if (m_pChild)
		m_pChild->Exit();
	m_pChild = _next;
	m_pChild->Enter();
}
