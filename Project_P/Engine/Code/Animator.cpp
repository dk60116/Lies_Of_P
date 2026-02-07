#include "epch.h"
#include "Animator.h"
#include "SkinnedMeshRenderer.h"
#include "Resources.h"

CAnimator::CAnimator()
	: m_pSkinnedRenderer(nullptr)
	, m_mAnimationList({})
	, m_pCrtAnimation(nullptr)
	, m_pNextAnimation(nullptr)
	, m_bApplyRootMotion(false)
	, m_bIsPlaying(false)
	, m_bBlending(false)
	, m_bLoop(false)
	, m_fCurrentTime(0.f)
	, m_fBlendTime(0.f)
	, m_fNextTime(0.f)
	, m_fBlendDuration(0.f)
	, m_fPlaybackSpeed(1.f)
	, m_iPrevTriggerFrame(-1)
	, m_pPrevTriggerClip(nullptr)
	, m_mActionHandlers({})
	, m_vFinalBoneMatrix({})
	, m_mBlendStartPose({})
	, m_sStateInfo({})
	, m_pController(nullptr)
	, m_pBlendTree(nullptr)
	, m_pNextBlendTree(nullptr)
	, m_bBlendTreeActive(false)
	, m_bNextBlendTreeActive(false)
	, m_pRootMotionParent(nullptr)
	, m_vPrevRootMotionPos(vector3::zero())
	, m_bHasPrevRootMotion(false)
	, m_vNextRootStartPos(vector3::zero())
	, m_bHasNextRootStartPos(false)
{
	m_strName = L"Animator";
	m_iSortIndex = 1;
}

CAnimator::~CAnimator()
{

}

CAnimator* CAnimator::Create()
{
	return new CAnimator();
}

CComponent* CAnimator::Clone() const
{
	CAnimator* clone = new CAnimator();

	clone->m_pCrtAnimation = this->m_pCrtAnimation;
	clone->m_pNextAnimation = this->m_pNextAnimation;
	clone->m_bApplyRootMotion = this->m_bApplyRootMotion;
	clone->m_bIsPlaying = this->m_bIsPlaying;
	clone->m_fCurrentTime = this->m_fCurrentTime;
	clone->m_fBlendTime = this->m_fBlendTime;
	clone->m_fBlendDuration = this->m_fBlendDuration;
	clone->m_fPlaybackSpeed = this->m_fPlaybackSpeed;
	clone->m_mActionHandlers = this->m_mActionHandlers;
	clone->m_vFinalBoneMatrix = this->m_vFinalBoneMatrix;
	clone->m_mBlendStartPose = this->m_mBlendStartPose;
	clone->m_sStateInfo = this->m_sStateInfo;
	clone->m_pBlendTree = this->m_pBlendTree;
	clone->m_pNextBlendTree = this->m_pNextBlendTree;
	clone->m_bBlendTreeActive = this->m_bBlendTreeActive;
	clone->m_bNextBlendTreeActive = this->m_bNextBlendTreeActive;

	clone->m_pController = this->m_pController;
	if (clone->m_pController)
		clone->m_pController->AddRef();

	for (TRAVERSAL_ITER(this->m_mAnimationList, it))
		clone->Add_Animation((*it).first, (*it).second);

	return clone;
}

HRESULT CAnimator::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	if (!m_pSkinnedRenderer)
	{
		m_pSkinnedRenderer = m_pGameObject->Get_Transform()
			->Get_Child(0)->Get_GameObject()
			->GetComponent<CSkinnedMeshRenderer>();

		if (m_pSkinnedRenderer)
			m_pSkinnedRenderer->AddRef();
	}

	if (m_pController)
	{
		m_ControllerInst.Initialize(m_pController, this, true);
	}

	return S_OK;
}

void CAnimator::Awake()
{
	if (m_pController)
		m_ControllerInst.Initialize(m_pController, this, true);
}

void CAnimator::Update()
{
	if (!m_pSkinnedRenderer)
		return;

	const _float dt = DELTA_TIME;	
	const _float prevCurrentTime = m_fCurrentTime;
	const _float prevNextTime = m_fNextTime;

	if (m_bIsPlaying && (m_pCrtAnimation || m_bBlendTreeActive))
	{
		m_fCurrentTime += dt * m_fPlaybackSpeed;

		const _float duration = m_bBlendTreeActive ? GetBlendTreeDuration(*m_pBlendTree) : m_pCrtAnimation->Get_Duration();
		m_sStateInfo.length = duration;

		if (duration > 0.f)
		{
			if (m_bLoop)
			{
				m_fCurrentTime = fmodf(m_fCurrentTime, duration);
			}
			else if (m_fCurrentTime >= duration)
			{
				m_fCurrentTime = duration;

				if (!m_bBlending)
					m_bIsPlaying = false;
			}

			m_sStateInfo.normalizeTime = m_fCurrentTime / duration;
		}
		else
		{
			m_sStateInfo.normalizeTime = 0.f;
		}
	}

	if (m_bBlending && (m_pNextAnimation || m_bNextBlendTreeActive))
	{
		m_fBlendTime += dt;

		m_fNextTime += dt * m_fPlaybackSpeed;

		const _float nextDur = m_bNextBlendTreeActive ? GetBlendTreeDuration(*m_pNextBlendTree) : m_pNextAnimation->Get_Duration();
		m_sStateInfo.length = nextDur;

		if (nextDur > 0.f)
		{
			if (m_bNextBlendTreeActive ? IsBlendTreeLoop(*m_pNextBlendTree) : m_pNextAnimation->IsLoop())
				m_fNextTime = fmodf(m_fNextTime, nextDur);
			else if (m_fNextTime >= nextDur)
				m_fNextTime = nextDur;

			m_sStateInfo.normalizeTime = m_fNextTime / nextDur;
		}
		else
		{
			m_sStateInfo.normalizeTime = 0.f;
		}
	}

	if (m_pController)
		m_ControllerInst.Update(this, dt);

	UpdateDirectBlendState(m_directBlendCurrent, m_bBlendTreeActive ? m_pBlendTree : nullptr, dt);
	UpdateDirectBlendState(m_directBlendNext, m_bNextBlendTreeActive ? m_pNextBlendTree : nullptr, dt);

	if (!m_pCrtAnimation && !m_bBlendTreeActive)
		return;
	if (!m_bIsPlaying && !m_bBlending)
		return;

	CAnimationClip* triggerClip = m_bBlendTreeActive ? GetBlendTreeDominantClip(*m_pBlendTree) : m_pCrtAnimation;
	_float prevTriggerTime = prevCurrentTime;
	_float currentTriggerTime = m_fCurrentTime;
	if (m_bBlending && (m_pNextAnimation || m_bNextBlendTreeActive))	
	{
		triggerClip = m_bNextBlendTreeActive ? GetBlendTreeDominantClip(*m_pNextBlendTree) : m_pNextAnimation;
		prevTriggerTime = prevNextTime;
		currentTriggerTime = m_fNextTime;
	}

	if (triggerClip != m_pPrevTriggerClip)
	{
		m_pPrevTriggerClip = triggerClip;
		m_iPrevTriggerFrame = -1;
	}
	ProcessActionTriggers(triggerClip, prevTriggerTime, currentTriggerTime);


	if (m_bBlending)
	{
		_bool rootMotionApplied = false;
		if (!m_pNextAnimation && !m_bNextBlendTreeActive)
		{
			m_bBlending = false;
			return;
		}

		_float denom = (m_fBlendDuration > 0.f) ? m_fBlendDuration : 0.0001f;
		_float t = m_fBlendTime / denom;

		if (t >= 1.f)
		{
			if (m_bNextBlendTreeActive)
			{
				m_pBlendTree = m_pNextBlendTree;
				m_bBlendTreeActive = true;
				m_pCrtAnimation = nullptr;
				m_pNextBlendTree = nullptr;
				m_bNextBlendTreeActive = false;
			}
			else
			{
				m_pCrtAnimation = m_pNextAnimation;
				m_bBlendTreeActive = false;
				m_pBlendTree = nullptr;
			}
			m_pNextAnimation = nullptr;

			m_fCurrentTime = m_fNextTime;
			m_fNextTime = 0.f;

			m_bBlending = false;
			m_fBlendTime = 0.f;
			m_fBlendDuration = 0.f;

			m_bIsPlaying = true;
			m_bLoop = m_bBlendTreeActive ? IsBlendTreeLoop(*m_pBlendTree) : ((m_pCrtAnimation) ? m_pCrtAnimation->IsLoop() : false);

			m_bHasPrevRootMotion = false;
			m_bHasNextRootStartPos = false;

			if (m_bBlendTreeActive || m_pCrtAnimation)
			{
				const _float dur = m_bBlendTreeActive ? GetBlendTreeDuration(*m_pBlendTree) : m_pCrtAnimation->Get_Duration();
				m_sStateInfo.length = dur;
				m_sStateInfo.normalizeTime = (dur > 0.f) ? (m_fCurrentTime / dur) : 0.f;

				if (!m_bLoop && dur > 0.f && m_fCurrentTime >= dur)
					m_bIsPlaying = false;
			}
			else
			{
				m_sStateInfo.length = 0.f;
				m_sStateInfo.normalizeTime = 0.f;
				m_bIsPlaying = false;
			}

			return;
		}

		unordered_map<wstring, CAnimationClip::BoneTransform> sampledNext;
		vector3 nextRootPos = vector3::zero();
		if (m_bNextBlendTreeActive)
			SampleBlendTreePose(*m_pNextBlendTree, m_fNextTime, sampledNext, &nextRootPos);
		else
			m_pNextAnimation->Sample(m_fNextTime, sampledNext);

		const _uint boneCount = m_pSkinnedRenderer->Get_BoneCount();
		for (_uint i = 0; i < boneCount; ++i)
		{
			CTransform* bone = m_pSkinnedRenderer->Get_BoneTransform(i);
			const wstring& name = m_pSkinnedRenderer->Get_BoneName(i);

			if (!bone)
				continue;
			
			if (m_bApplyRootMotion)
			{
				if (IsRootBone(name))
				{
						if (!rootMotionApplied && m_pRootMotionParent)
						{
							const auto startIt = m_mBlendStartPose.find(name);
							const auto nextIt = sampledNext.find(name);
							if (startIt != m_mBlendStartPose.end() && nextIt != sampledNext.end())
							{
								const auto& btStart = startIt->second;
								const auto& btNext = nextIt->second;
								vector3 rootPos = vector3::Lerp(btStart.pos, btNext.pos, t);
								if (m_bHasNextRootStartPos)
								{
									const vector3 offset = btStart.pos - m_vNextRootStartPos;
									rootPos = vector3::Lerp(btStart.pos, btNext.pos + offset, t);
								}
								ApplyRootMotionDelta(rootPos);
								rootMotionApplied = true;
							}
						}
					continue;
				}
			}

			const auto startIt = m_mBlendStartPose.find(name);
			const auto nextIt = sampledNext.find(name);

			if (startIt != m_mBlendStartPose.end() && nextIt != sampledNext.end())
			{
				const auto& btStart = startIt->second;
				const auto& btNext = nextIt->second;

				vector3 pos = vector3::Lerp(btStart.pos, btNext.pos, t);
				vector3 scale = vector3::Lerp(btStart.scale, btNext.scale, t);
				quaternion rot = quaternion::Slerp(btStart.rot, btNext.rot, t);

				bone->Set_LocalPosition(pos);
				bone->Set_LocalQuaternion(rot);
				bone->Set_LocalScale(scale);
			}
		}
		return;
	}

	unordered_map<wstring, CAnimationClip::BoneTransform> sampled;
	vector3 rootPos = vector3::zero();
	if (m_bBlendTreeActive)
		SampleBlendTreePose(*m_pBlendTree, m_fCurrentTime, sampled, &rootPos);
	else
		m_pCrtAnimation->Sample(m_fCurrentTime, sampled);

	const _uint boneCount = m_pSkinnedRenderer->Get_BoneCount();
	_bool rootMotionApplied = false;

	for (_uint i = 0; i < boneCount; ++i)
	{
		CTransform* bone = m_pSkinnedRenderer->Get_BoneTransform(i);

		if (!bone)
			continue;

		const wstring& name = m_pSkinnedRenderer->Get_BoneName(i);

		if (m_bApplyRootMotion)
		{
			if (IsRootBone(name))
			{
				if (m_pRootMotionParent)
				{
					if (!rootMotionApplied)
					{
						if (m_bBlendTreeActive)
							ApplyRootMotionDelta(rootPos);
						else
						{
							auto rootIt = sampled.find(name);
							if (rootIt != sampled.end())
								ApplyRootMotionDelta(rootIt->second.pos);
						}
						rootMotionApplied = true;
					}
				}
				continue;
			}
		}

		auto it = sampled.find(name);
		
		if (it == sampled.end())
			continue;

		const auto& bt = it->second;
		bone->Set_LocalPosition(bt.pos);
		bone->Set_LocalQuaternion(bt.rot);
		bone->Set_LocalScale(bt.scale);
	}
}

void CAnimator::OnDestroy()
{
	for (TRAVERSAL_ITER(m_mAnimationList, it))
		Safe_Release((*it).second);
	m_mAnimationList.clear();

	m_ControllerInst.OnDestroy();

	Safe_Release(m_pController);
	Safe_Release(m_pSkinnedRenderer);
	Safe_Release(m_pRootMotionParent);
}

void CAnimator::ApplyRootMotionDelta(const vector3& rootPos)
{
	if (!m_pRootMotionParent)
		return;

	if (!m_bHasPrevRootMotion)
	{
		m_vPrevRootMotionPos = rootPos;
		m_bHasPrevRootMotion = true;
		return;
	}

	vector3 delta = rootPos - m_vPrevRootMotionPos;
	vector3 mapped = vector3(delta.z, delta.y, -delta.x);
	const auto& directions = m_pRootMotionParent->Get_Directions();
	vector3 worldDelta = (directions.right * mapped.x + directions.up * mapped.y + directions.forward * mapped.z) * m_pSkinnedRenderer->Get_SkinnedMeshBuffer()->Get_ScaleFactor();
	m_pRootMotionParent->Add_LocalPosition(-worldDelta);
	m_vPrevRootMotionPos = rootPos;
}

const _bool CAnimator::IsLoop() const
{
	return m_bLoop;
}

const _bool CAnimator::ApplyRootmotion() const
{
	return m_bApplyRootMotion;
}

void CAnimator::SetApplyRootmotion(const _bool _value, CTransform* _target)
{
	m_bApplyRootMotion = _value;
	m_bHasPrevRootMotion = false;

	if (m_pRootMotionParent)
	{
		Safe_Release(m_pRootMotionParent);
		m_pRootMotionParent = nullptr;
	}

	if (_value)
	{
		m_pRootMotionParent = _target;

		if (_target)
			_target->AddRef();
	}
}

void CAnimator::Set_PlaybackSpeed(const _float _value)
{
	m_fPlaybackSpeed = _value;
}

void CAnimator::Add_Animation(const wstring& _animName, CAnimationClip* _anim)
{
	if (!_anim)
	{
		CDebug::LogError(L"Add Animation failed - Animation is nullptr: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	m_mAnimationList[_animName] = _anim;

	_anim->AddRef();
}

const _bool CAnimator::IsPlaying() const
{
	return m_bIsPlaying;
}

void CAnimator::Play()
{
	if (!m_pSkinnedRenderer)
	{
		CDebug::LogError(L"Animator play failed - Skinned renderer is nullptr: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (m_pCrtAnimation)
	{
		m_bLoop = m_pCrtAnimation->IsLoop();
		m_bIsPlaying = true;
		m_bHasPrevRootMotion = false;
	}
	if (m_bBlendTreeActive && m_pBlendTree)
	{
		m_bLoop = IsBlendTreeLoop(*m_pBlendTree);
		m_bIsPlaying = true;
		m_bHasPrevRootMotion = false;
	}
}

void CAnimator::Play(const wstring& _animName, const _float _blendDuration, _bool _restartSame)
{
	auto iter = m_mAnimationList.find(_animName);

	if (!m_pSkinnedRenderer)
	{
		CDebug::LogError(L"Animator play failed - Skinned renderer is nullptr: " + _animName + L" - " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (iter == m_mAnimationList.end())
	{
		CDebug::LogError(L"Animator play failed - Animation not found: " + _animName + L" - " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	CAnimationClip* nextAnim = iter->second;

	if (_restartSame && nextAnim && nextAnim == m_pCrtAnimation && !m_bBlending)
	{
		m_pNextAnimation = nullptr;
		m_pNextBlendTree = nullptr;
		m_bNextBlendTreeActive = false;
		m_bBlendTreeActive = false;
		m_pBlendTree = nullptr;
		m_fCurrentTime = 0.f;
		m_fNextTime = 0.f;

		m_bIsPlaying = true;
		m_bBlending = false;
		m_fBlendTime = 0.f;
		m_fBlendDuration = 0.f;

		m_bHasPrevRootMotion = false;
		m_bHasNextRootStartPos = false;
		ResetActionTriggerState();

		m_bLoop = m_pCrtAnimation->IsLoop();
		return;
	}

	if (_blendDuration <= 0.f || !m_pCrtAnimation)
	{
		m_pCrtAnimation = nextAnim;
		m_pNextAnimation = nullptr;
		m_pNextBlendTree = nullptr;
		m_bNextBlendTreeActive = false;
		m_bBlendTreeActive = false;
		m_pBlendTree = nullptr;

		m_fCurrentTime = 0.f;
		m_fNextTime = 0.f;

		m_bIsPlaying = true;
		m_bBlending = false;
		m_fBlendTime = 0.f;
		m_fBlendDuration = 0.f;
		m_bHasPrevRootMotion = false;
		ResetActionTriggerState();

		m_bLoop = m_pCrtAnimation->IsLoop();
		return;
	}

	m_pNextAnimation = nextAnim;
	m_pNextBlendTree = nullptr;
	m_bNextBlendTreeActive = false;
	m_fBlendTime = 0.f;
	m_fBlendDuration = _blendDuration;
	m_bBlending = true;

	m_fNextTime = 0.f;
	m_bHasNextRootStartPos = false;

	if (!m_bIsPlaying)
		m_fCurrentTime = 0.f;

	m_mBlendStartPose.clear();
	if (m_bBlendTreeActive)
	{
		vector3 rootPos = vector3::zero();
		SampleBlendTreePose(*m_pBlendTree, m_fCurrentTime, m_mBlendStartPose, &rootPos);
	}
	else
		m_pCrtAnimation->Sample(m_fCurrentTime, m_mBlendStartPose);
	unordered_map<wstring, CAnimationClip::BoneTransform> nextStartPose;
	m_pNextAnimation->Sample(0.f, nextStartPose);
	for (const auto& item : nextStartPose)
	{
		if (IsRootBone(item.first))
		{
			m_vNextRootStartPos = item.second.pos;
			m_bHasNextRootStartPos = true;
			break;
		}
	}
	m_bHasPrevRootMotion = false;

	m_bIsPlaying = true;
}

void CAnimator::PlayBlendTree(const CAnimatorController::State::BlendTree& _blendTree, const _float _blendDuration, const _bool _restartSame)
{
	if (!m_pSkinnedRenderer)
	{
		CDebug::LogError(L"Animator play blend tree failed - Skinned renderer is nullptr: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (_restartSame && m_bBlendTreeActive && m_pBlendTree == &_blendTree && !m_bBlending)
	{
		m_pNextAnimation = nullptr;
		m_pNextBlendTree = nullptr;
		m_bNextBlendTreeActive = false;
		m_fCurrentTime = 0.f;
		m_fNextTime = 0.f;

		m_bIsPlaying = true;
		m_bBlending = false;
		m_fBlendTime = 0.f;
		m_fBlendDuration = 0.f;

		m_bHasPrevRootMotion = false;
		m_bHasNextRootStartPos = false;
		ResetActionTriggerState();

		m_bLoop = IsBlendTreeLoop(_blendTree);
		return;
	}

	if (_blendDuration <= 0.f || (!m_pCrtAnimation && !m_bBlendTreeActive))
	{
		m_pBlendTree = &_blendTree;
		m_bBlendTreeActive = true;
		m_pCrtAnimation = nullptr;
		m_pNextAnimation = nullptr;
		m_pNextBlendTree = nullptr;
		m_bNextBlendTreeActive = false;

		m_fCurrentTime = 0.f;
		m_fNextTime = 0.f;

		m_bIsPlaying = true;
		m_bBlending = false;
		m_fBlendTime = 0.f;
		m_fBlendDuration = 0.f;
		m_bHasPrevRootMotion = false;
		ResetActionTriggerState();

		m_bLoop = IsBlendTreeLoop(_blendTree);
		return;
	}

	m_pNextBlendTree = &_blendTree;
	m_bNextBlendTreeActive = true;
	m_pNextAnimation = nullptr;
	m_fBlendTime = 0.f;
	m_fBlendDuration = _blendDuration;
	m_bBlending = true;

	m_fNextTime = 0.f;
	m_bHasNextRootStartPos = false;

	if (!m_bIsPlaying)
		m_fCurrentTime = 0.f;

	m_mBlendStartPose.clear();
	if (m_bBlendTreeActive)
	{
		vector3 rootPos = vector3::zero();
		SampleBlendTreePose(*m_pBlendTree, m_fCurrentTime, m_mBlendStartPose, &rootPos);
	}
	else if (m_pCrtAnimation)
		m_pCrtAnimation->Sample(m_fCurrentTime, m_mBlendStartPose);

	unordered_map<wstring, CAnimationClip::BoneTransform> nextStartPose;
	vector3 nextRootPos = vector3::zero();
	SampleBlendTreePose(_blendTree, 0.f, nextStartPose, &nextRootPos);
	for (const auto& item : nextStartPose)
	{
		if (IsRootBone(item.first))
		{
			m_vNextRootStartPos = item.second.pos;
			m_bHasNextRootStartPos = true;
			break;
		}
	}
	m_bHasPrevRootMotion = false;

	m_bIsPlaying = true;
}

void CAnimator::Pause()
{
	m_bIsPlaying = false;
}

void CAnimator::Stop()
{
	m_fCurrentTime = 0.f;
	m_bHasPrevRootMotion = false;
	ResetActionTriggerState();
	Update();
	m_bIsPlaying = false;
}

void CAnimator::Set_Controller(CAnimatorController* _controller, const _bool _playEntry)
{
	(void)_playEntry;
	if (m_pController == _controller)
		return;

	m_ControllerInst.OnDestroy();
	Safe_Release(m_pController);

	m_pController = _controller;

	if (m_pController)
	{
		m_pController->AddRef();
		for (const auto& statePair : m_pController->Get_StateMap())
		{
			const auto& state = statePair.second;
			if (state.motionType == CAnimatorController::STATE_MOTION_TYPE::BLEND_TREE)
			{
				for (const auto& child : state.blendTree.children)
				{
					if (child.motionName.empty())
						continue;
					if (m_mAnimationList.find(child.motionName) != m_mAnimationList.end())
						continue;
					wstring clipName = child.motionName + L" (Animation Clip)";
					CAnimationClip* clip = CResources::GetInstance().LoadOnScene<CAnimationClip>(clipName);
					if (clip)
						Add_Animation(child.motionName, clip);
				}
				continue;
			}

			if (!state.motionName.empty())
			{
				if (m_mAnimationList.find(state.motionName) != m_mAnimationList.end())
					continue;

				wstring clipName = state.motionName + L" (Animation Clip)";
				CAnimationClip* clip = CResources::GetInstance().LoadOnScene<CAnimationClip>(clipName);
				if (clip)
					Add_Animation(state.motionName, clip);
			}
		}
		m_ControllerInst.Initialize(m_pController, this, true);
	}
}

unordered_map<wstring, CAnimationClip*>& CAnimator::Get_AnimationClipList()
{
	return m_mAnimationList;
}

CAnimationClip* CAnimator::Get_CurrentAnimation()
{
	return m_pCrtAnimation;
}

CAnimator:: AnimatorStateInfo& CAnimator::Get_StateInfo()
{
	return m_sStateInfo;
}

const _float CAnimator::GetNormalizedTime() const
{
	const CAnimationClip* clip = nullptr;
	_float time = 0.f;

	if (m_bBlending && (m_pNextAnimation || m_bNextBlendTreeActive))
	{
		time = m_fNextTime;
		if (m_bNextBlendTreeActive)
		{
			const _float dur = GetBlendTreeDuration(*m_pNextBlendTree);
			if (dur <= 0.f)
				return 0.f;
			_float t = time / dur;
			if (IsBlendTreeLoop(*m_pNextBlendTree))
			{
				const _float wrapped = fmodf(time, dur);
				t = wrapped / dur;
			}
			if (!(t == t))
				return 0.f;
			return clamp(t, 0.f, 1.f);
		}
		clip = m_pNextAnimation;
	}
	else
	{
		time = m_fCurrentTime;
		if (m_bBlendTreeActive)
		{
			const _float dur = GetBlendTreeDuration(*m_pBlendTree);
			if (dur <= 0.f)
				return 0.f;
			_float t = time / dur;
			if (IsBlendTreeLoop(*m_pBlendTree))
			{
				const _float wrapped = fmodf(time, dur);
				t = wrapped / dur;
			}
			if (!(t == t))
				return 0.f;
			return clamp(t, 0.f, 1.f);
		}
		clip = m_pCrtAnimation;
	}

	if (!clip)
		return 0.f;

	const _float dur = clip->Get_Duration();
	if (dur <= 0.f)
		return 0.f;

	_float t = time / dur;

	if (clip->IsLoop())
	{
		const _float wrapped = fmodf(time, dur);
		t = wrapped / dur;
	}
	if (!(t == t))
		return 0.f;

	return clamp(t, 0.f, 1.f);
}

_bool CAnimator::IsRootBone(const wstring& _name)
{
	_bool result = false;

	auto& routs = m_pSkinnedRenderer->GetRootBons();

	for (size_t i = 0; i < routs.size(); ++i)
	{
		if (routs[i]->Get_GameObject()->Get_ObjectName() == _name)
			result = true;
	}

	return result;
}

void CAnimator::SetBool(const wstring& n, _bool v)
{
	m_ControllerInst.SetBool(n, v);
}

void CAnimator::SetInt(const wstring& n, _int v)
{
	m_ControllerInst.SetInt(n, v);
}

void CAnimator::SetFloat(const wstring& n, _float v)
{
	m_ControllerInst.SetFloat(n, v);
}

void CAnimator::ProcessActionTriggers(CAnimationClip* clip, _float prevTime, _float currentTime)
{
	if (!clip)
		return;

	const auto& triggers = clip->Get_ActionTriggerList();
	if (triggers.empty())
		return;

	const _float ticksPerSecond = clip->Get_TickPerSecons();
	if (ticksPerSecond <= 0.f)
		return;

	const _float duration = clip->Get_Duration();
	if (duration <= 0.f)
		return;

	const _float totalTicks = duration * ticksPerSecond;
	if (totalTicks <= 0.f)
		return;

	auto calcFrame = [totalTicks, ticksPerSecond](const _float time, const _bool looped)
	{
		_float ticks = time * ticksPerSecond;
		if (looped)
			ticks = fmodf(ticks, totalTicks);
		else if (ticks > totalTicks)
			ticks = totalTicks;
		if (ticks < 0.f)
			ticks = 0.f;
		return static_cast<_int>(floor(ticks));
	};

	const _bool looped = clip->IsLoop();
	_uint currentFrame = calcFrame(currentTime, looped);
	_uint prevFrame = m_iPrevTriggerFrame;
	if (prevFrame < 0)
		prevFrame = currentFrame - 1;

	if (looped && currentFrame < prevFrame)
	{
		const _int lastFrame = static_cast<_int>(floor(totalTicks));
		for (const auto& trigger : triggers)
		{
			if (trigger.actionName.empty())
				continue;
			if ((trigger.frame > prevFrame && trigger.frame <= lastFrame) || (trigger.frame >= 0 && trigger.frame <= currentFrame))
			{
				const auto handlerIt = m_mActionHandlers.find(trigger.actionName);
				if (handlerIt != m_mActionHandlers.end() && handlerIt->second)
					handlerIt->second();
				m_ControllerInst.SetTrigger(trigger.actionName);
			}
		}
	}
	else if (currentFrame != prevFrame)
	{
		for (const auto& trigger : triggers)
		{
			if (trigger.actionName.empty())
				continue;
			if (trigger.frame > prevFrame && trigger.frame <= currentFrame)
			{
				const auto handlerIt = m_mActionHandlers.find(trigger.actionName);
				if (handlerIt != m_mActionHandlers.end() && handlerIt->second)
					handlerIt->second();
				m_ControllerInst.SetTrigger(trigger.actionName);
			}
		}
	}

	m_iPrevTriggerFrame = currentFrame;
}

void CAnimator::ResetActionTriggerState()
{
	m_iPrevTriggerFrame = -1;
	m_pPrevTriggerClip = nullptr;
}

_float CAnimator::GetParamValue(const wstring& name) const
{
	_float value = 0.f;
	if (m_ControllerInst.TryGetParamValue(name, value))
		return value;
	return 0.f;
}

void CAnimator::UpdateDirectBlendState(DirectBlendState& state, const CAnimatorController::State::BlendTree* tree, _float dt)
{
	if (!tree || tree->type != CAnimatorController::BLEND_TREE_TYPE::DIRECT || tree->paramX.empty() || tree->directBlendDuration <= 0.f)
	{
		state.tree = tree;
		state.active = false;
		state.hasValue = false;
		state.timer = 0.f;
		state.duration = 0.f;
		return;
	}

	if (state.tree != tree)
	{
		state = DirectBlendState{};
		state.tree = tree;
	}

	state.duration = tree->directBlendDuration;
	_float value = GetParamValue(tree->paramX);

	if (!state.hasValue)
	{
		state.current = value;
		state.target = value;
		state.start = value;
		state.timer = 0.f;
		state.active = false;
		state.hasValue = true;
		return;
	}

	if (value != state.target)
	{
		state.start = state.current;
		state.target = value;
		state.timer = 0.f;
		state.active = true;
	}

	if (state.active)
	{
		state.timer += dt;
		_float denom = state.duration > 0.f ? state.duration : 0.0001f;
		_float t = state.timer / denom;
		if (t >= 1.f)
		{
			state.current = state.target;
			state.active = false;
		}
		else
		{
			state.current = state.start + (state.target - state.start) * t;
		}
	}
	else
	{
		state.current = state.target;
	}
}

_float CAnimator::GetDirectBlendParamValue(const CAnimatorController::State::BlendTree& tree) const
{
	if (tree.type != CAnimatorController::BLEND_TREE_TYPE::DIRECT || tree.paramX.empty() || tree.directBlendDuration <= 0.f)
		return GetParamValue(tree.paramX);

	if (m_directBlendCurrent.tree == &tree && m_directBlendCurrent.hasValue)
		return m_directBlendCurrent.current;
	if (m_directBlendNext.tree == &tree && m_directBlendNext.hasValue)
		return m_directBlendNext.current;
	return GetParamValue(tree.paramX);
}

void CAnimator::ComputeBlendTreeWeights(const CAnimatorController::State::BlendTree& tree, vector<_float>& weights, vector<const CAnimatorController::State::BlendTreeChild*>& children) const
{
	weights.clear();
	children.clear();
	for (const auto& child : tree.children)
		children.push_back(&child);

	if (children.empty())
		return;

	weights.resize(children.size(), 0.f);

	if (tree.type == CAnimatorController::BLEND_TREE_TYPE::ONE_D)
	{
		vector<size_t> indices(children.size());
		for (size_t i = 0; i < children.size(); ++i)
			indices[i] = i;

		sort(indices.begin(), indices.end(), [&](size_t a, size_t b)
			{
				return children[a]->threshold < children[b]->threshold;
			});

		_float value = GetParamValue(tree.paramX);
		size_t first = indices.front();
		size_t last = indices.back();
		if (value <= children[first]->threshold)
		{
			weights[first] = 1.f;
			return;
		}
		if (value >= children[last]->threshold)
		{
			weights[last] = 1.f;
			return;
		}

		for (size_t i = 0; i + 1 < indices.size(); ++i)
		{
			size_t a = indices[i];
			size_t b = indices[i + 1];
			_float t1 = children[a]->threshold;
			_float t2 = children[b]->threshold;
			if (value >= t1 && value <= t2 && t2 != t1)
			{
				_float t = (value - t1) / (t2 - t1);
				weights[a] = 1.f - t;
				weights[b] = t;
				return;
			}
		}
		return;
	}

	if (tree.type == CAnimatorController::BLEND_TREE_TYPE::TWO_D)
	{
		_float x = GetParamValue(tree.paramX);
		_float y = GetParamValue(tree.paramY);
		_float total = 0.f;
		for (size_t i = 0; i < children.size(); ++i)
		{
			_float dx = children[i]->position.x - x;
			_float dy = children[i]->position.y - y;
			_float dist = sqrtf(dx * dx + dy * dy);
			_float w = 1.f / (dist + 0.001f);
			weights[i] = w;
			total += w;
		}
		if (total > 0.f)
		{
			for (auto& w : weights)
				w /= total;
		}
		return;
	}

	if (tree.type == CAnimatorController::BLEND_TREE_TYPE::DIRECT)
	{
		if (!tree.paramX.empty())
		{
			_float x = GetDirectBlendParamValue(tree);
			vector<_float> thresholds;
			thresholds.reserve(children.size());
			for (const auto* child : children)
				thresholds.push_back(child->threshold);

			vector<size_t> order(thresholds.size());
			for (size_t i = 0; i < order.size(); ++i)
				order[i] = i;

			std::sort(order.begin(), order.end(), [&](size_t a, size_t b)
				{
					return thresholds[a] < thresholds[b];
				});

			if (order.size() == 1)
			{
				weights[order[0]] = 1.f;
				return;
			}

			if (x <= thresholds[order.front()])
			{
				weights[order.front()] = 1.f;
				return;
			}
			if (x >= thresholds[order.back()])
			{
				weights[order.back()] = 1.f;
				return;
			}

			for (size_t i = 0; i + 1 < order.size(); ++i)
			{
				size_t a = order[i];
				size_t b = order[i + 1];
				_float t0 = thresholds[a];
				_float t1 = thresholds[b];
				if (x >= t0 && x <= t1)
				{
					_float range = t1 - t0;
					_float t = (range <= 0.f) ? 0.f : (x - t0) / range;
					weights[a] = 1.f - t;
					weights[b] = t;
					return;
				}
			}
			return;
		}

		_float total = 0.f;
		for (size_t i = 0; i < children.size(); ++i)
		{
			_float w = GetParamValue(children[i]->directParam);
			if (w < 0.f)
				w = 0.f;
			weights[i] = w;
			total += w;
		}
		if (total <= 0.f)
		{
			_float w = 1.f / static_cast<_float>(children.size());
			for (auto& v : weights)
				v = w;
			return;
		}
		for (auto& v : weights)
			v /= total;
	}
}

_float CAnimator::GetBlendTreeDuration(const CAnimatorController::State::BlendTree& tree) const
{
	_float duration = 0.f;
	for (const auto& child : tree.children)
	{
		auto it = m_mAnimationList.find(child.motionName);
		if (it == m_mAnimationList.end())
			continue;
		_float d = it->second->Get_Duration();
		if (d > duration)
			duration = d;
	}
	return duration;
}

_bool CAnimator::IsBlendTreeLoop(const CAnimatorController::State::BlendTree& tree) const
{
	for (const auto& child : tree.children)
	{
		auto it = m_mAnimationList.find(child.motionName);
		if (it == m_mAnimationList.end())
			continue;
		if (it->second->IsLoop())
			return true;
	}
	return false;
}

CAnimationClip* CAnimator::GetBlendTreeDominantClip(const CAnimatorController::State::BlendTree& tree) const
{
	vector<_float> weights;
	vector<const CAnimatorController::State::BlendTreeChild*> children;
	ComputeBlendTreeWeights(tree, weights, children);
	if (children.empty())
		return nullptr;

	size_t best = 0;
	for (size_t i = 1; i < weights.size(); ++i)
	{
		if (weights[i] > weights[best])
			best = i;
	}

	auto it = m_mAnimationList.find(children[best]->motionName);
	if (it == m_mAnimationList.end())
		return nullptr;
	return it->second;
}

void CAnimator::SampleBlendTreePose(const CAnimatorController::State::BlendTree& tree, _float time, unordered_map<wstring, CAnimationClip::BoneTransform>& outPose, vector3* outRootPos)
{
	outPose.clear();
	if (outRootPos)
		*outRootPos = vector3::zero();

	vector<_float> weights;
	vector<const CAnimatorController::State::BlendTreeChild*> children;
	ComputeBlendTreeWeights(tree, weights, children);
	if (children.empty())
		return;

	_float totalWeight = 0.f;
	for (size_t i = 0; i < children.size(); ++i)
	{
		auto it = m_mAnimationList.find(children[i]->motionName);
		if (it == m_mAnimationList.end())
			continue;
		_float w = weights[i];
		if (w <= 0.f)
			continue;

		unordered_map<wstring, CAnimationClip::BoneTransform> sampled;
		it->second->Sample(time, sampled);
		if (outPose.empty())
		{
			for (const auto& kv : sampled)
				outPose[kv.first] = kv.second;
			totalWeight = w;
		}
		else
		{
			_float t = totalWeight > 0.f ? (w / (totalWeight + w)) : 1.f;
			for (const auto& kv : sampled)
			{
				auto poseIt = outPose.find(kv.first);
				if (poseIt == outPose.end())
				{
					outPose[kv.first] = kv.second;
					continue;
				}
				auto& current = poseIt->second;
				const auto& next = kv.second;
				current.pos = vector3::Lerp(current.pos, next.pos, t);
				current.scale = vector3::Lerp(current.scale, next.scale, t);
				current.rot = quaternion::Slerp(current.rot, next.rot, t);
			}
			totalWeight += w;
		}

		if (outRootPos)
		{
			for (const auto& kv : sampled)
			{
				if (IsRootBone(kv.first))
				{
					if (totalWeight <= w)
						*outRootPos = kv.second.pos;
					else
						*outRootPos = vector3::Lerp(*outRootPos, kv.second.pos, w / totalWeight);
					break;
				}
			}
		}
	}
}

void CAnimator::SetTrigger(const wstring& n)
{
	m_ControllerInst.SetTrigger(n);
}

void CAnimator::ResetTrigger(const wstring& n)
{
	m_ControllerInst.ResetTrigger(n);
}

void CAnimator::RegisterActionHandler(const wstring& name, const function<void()>& handler)
{
	m_mActionHandlers[name] = handler;
}

void CAnimator::UnregisterActionHandler(const wstring& name)
{
	m_mActionHandlers.erase(name);
}

void CAnimator::ClearActionHandlers()
{
	m_mActionHandlers.clear();
}

const _bool CAnimator::GetBool(const wstring& n, _bool& out) const
{
	_bool r = m_ControllerInst.GetBool(n, out);
	if (!r)
		CDebug::LogError(L"Not fount Animation Clip bool Value - \"" + n + L'"' + L": " + m_pGameObject->Get_ObjectNameID());
	return r;
}

const _bool CAnimator::GetInt(const wstring& n, _bool& out) const
{
	_bool r = m_ControllerInst.GetInt(n, out);
	if (!r)
		CDebug::LogError(L"Not fount Animation Clip Int Value - \"" + n + L'"' + L": " + m_pGameObject->Get_ObjectNameID());
	return r;
}

const _bool CAnimator::GetFloat(const wstring& n, _float& out) const
{
	_bool r = m_ControllerInst.GetFloat(n, out);
	if (!r)
		CDebug::LogError(L"Not fount Animation Clip Float Value - \"" + n + L'"' + L": " + m_pGameObject->Get_ObjectNameID());
	return r;
}
