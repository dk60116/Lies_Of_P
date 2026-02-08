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

void CPlayerState_Locomotion::Initialize(CPlayerControllerContext* _context)
{
    __super::Initialize(_context);

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

    m_pCtx->TickActionBuffer(CPlayerController::PlayerState::Attack);
    m_pCtx->TickActionBuffer(CPlayerController::PlayerState::Guard);
    m_pCtx->TickActionBuffer(CPlayerController::PlayerState::Evade);


    if (m_pCtx->HasActionBuffered(CPlayerController::PlayerState::Evade))
    {
        m_pCtx->ConsumeActionBuffer(CPlayerController::PlayerState::Evade);
        TransitionTo(m_mChildList[CPlayerController::PlayerState::Evade]);
    }
    else if (m_pCtx->HasActionBuffered(CPlayerController::PlayerState::Guard))
    {
        m_pCtx->ConsumeActionBuffer(CPlayerController::PlayerState::Guard);
        TransitionTo(m_mChildList[CPlayerController::PlayerState::Guard]);
    }
    else if (m_pCtx->HasActionBuffered(CPlayerController::PlayerState::Attack))
    {
        if (m_pCtx->IsActionActive(CPlayerController::PlayerState::Guard) && !m_pCtx->IsCanAttack())
        {
            m_pCtx->ConsumeActionBuffer(CPlayerController::PlayerState::Attack);
        }
        else
        {
            m_pCtx->ConsumeActionBuffer(CPlayerController::PlayerState::Attack);
            TransitionTo(m_mChildList[CPlayerController::PlayerState::Attack]);
        }
    }
    else if (m_pCtx->IsActionActive(CPlayerController::PlayerState::Evade))
        TransitionTo(m_mChildList[CPlayerController::PlayerState::Evade]);
    else if (m_pCtx->IsActionActive(CPlayerController::PlayerState::Guard))
        TransitionTo(m_mChildList[CPlayerController::PlayerState::Guard]);
    else if (m_pCtx->IsActionActive(CPlayerController::PlayerState::Attack))
        TransitionTo(m_mChildList[CPlayerController::PlayerState::Attack]);
    else
        TransitionTo(m_pCtx->IsKeyPressed_Hold(CPlayerController::PlayerState::Move)
            ? m_mChildList[CPlayerController::PlayerState::Move]
            : m_mChildList[CPlayerController::PlayerState::Idle]);

    if (m_pChild) m_pChild->Update();

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
