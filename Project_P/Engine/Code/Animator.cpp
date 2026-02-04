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
	, m_vFinalBoneMatrix({})
	, m_mBlendStartPose({})
	, m_sStateInfo({})
	, m_pController(nullptr)
	, m_pRootMotionParent(nullptr)
	, m_vPrevRootMotionPos(vector3::zero())
	, m_bHasPrevRootMotion(false)
	, m_vNextRootStartPos(vector3::zero())
	, m_bHasNextRootStartPos(false)
{
	m_strName = L"Animator";
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
	clone->m_vFinalBoneMatrix = this->m_vFinalBoneMatrix;
	clone->m_mBlendStartPose = this->m_mBlendStartPose;
	clone->m_sStateInfo = this->m_sStateInfo;

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

	if (m_bIsPlaying && m_pCrtAnimation)
	{
		m_fCurrentTime += dt * m_fPlaybackSpeed;

		const _float duration = m_pCrtAnimation->Get_Duration();
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

	if (m_bBlending && m_pNextAnimation)
	{
		m_fBlendTime += dt;

		m_fNextTime += dt * m_fPlaybackSpeed;

		const _float nextDur = m_pNextAnimation->Get_Duration();
		m_sStateInfo.length = nextDur;

		if (nextDur > 0.f)
		{
			if (m_pNextAnimation->IsLoop())
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

	if (!m_pCrtAnimation)
		return;
	if (!m_bIsPlaying && !m_bBlending)
		return;

	if (m_bBlending)
	{
		_bool rootMotionApplied = false;
		if (!m_pNextAnimation)
		{
			m_bBlending = false;
			return;
		}

		_float denom = (m_fBlendDuration > 0.f) ? m_fBlendDuration : 0.0001f;
		_float t = m_fBlendTime / denom;

		if (t >= 1.f)
		{
			m_pCrtAnimation = m_pNextAnimation;
			m_pNextAnimation = nullptr;

			m_fCurrentTime = m_fNextTime;
			m_fNextTime = 0.f;

			m_bBlending = false;
			m_fBlendTime = 0.f;
			m_fBlendDuration = 0.f;

			m_bIsPlaying = true;
			m_bLoop = (m_pCrtAnimation) ? m_pCrtAnimation->IsLoop() : false;

			m_bHasPrevRootMotion = false;
			m_bHasNextRootStartPos = false;

			if (m_pCrtAnimation)
			{
				const _float dur = m_pCrtAnimation->Get_Duration();
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
						auto rootIt = sampled.find(name);
						if (rootIt != sampled.end())
						{
							ApplyRootMotionDelta(rootIt->second.pos);
							rootMotionApplied = true;
						}
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
		m_fCurrentTime = 0.f;
		m_fNextTime = 0.f;

		m_bIsPlaying = true;
		m_bBlending = false;
		m_fBlendTime = 0.f;
		m_fBlendDuration = 0.f;

		m_bHasPrevRootMotion = false;
		m_bHasNextRootStartPos = false;

		m_bLoop = m_pCrtAnimation->IsLoop();
		return;
	}

	if (_blendDuration <= 0.f || !m_pCrtAnimation)
	{
		m_pCrtAnimation = nextAnim;
		m_pNextAnimation = nullptr;

		m_fCurrentTime = 0.f;
		m_fNextTime = 0.f;

		m_bIsPlaying = true;
		m_bBlending = false;
		m_fBlendTime = 0.f;
		m_fBlendDuration = 0.f;
		m_bHasPrevRootMotion = false;

		m_bLoop = m_pCrtAnimation->IsLoop();
		return;
	}

	m_pNextAnimation = nextAnim;
	m_fBlendTime = 0.f;
	m_fBlendDuration = _blendDuration;
	m_bBlending = true;

	m_fNextTime = 0.f;
	m_bHasNextRootStartPos = false;

	if (!m_bIsPlaying)
		m_fCurrentTime = 0.f;

	m_mBlendStartPose.clear();
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

void CAnimator::Pause()
{
	m_bIsPlaying = false;
}

void CAnimator::Stop()
{
	m_fCurrentTime = 0.f;
	m_bHasPrevRootMotion = false;
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
			if (state.motionName.empty())
				continue;
			if (m_mAnimationList.find(state.motionName) != m_mAnimationList.end())
				continue;

			wstring clipName = state.motionName + L" (Animation Clip)";
			CAnimationClip* clip = CResources::GetInstance().LoadOnScene<CAnimationClip>(clipName);
			if (clip)
				Add_Animation(state.motionName, clip);
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

	if (m_bBlending && m_pNextAnimation)
	{
		clip = m_pNextAnimation;
		time = m_fNextTime;
	}
	else
	{
		clip = m_pCrtAnimation;
		time = m_fCurrentTime;
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

void CAnimator::SetFloat(const wstring& n, _float v)
{
	m_ControllerInst.SetFloat(n, v);
}

void CAnimator::SetTrigger(const wstring& n)
{
	m_ControllerInst.SetTrigger(n);
}

void CAnimator::ResetTrigger(const wstring& n)
{
	m_ControllerInst.ResetTrigger(n);
}

const _bool CAnimator::GetBool(const wstring& n, _bool& out) const
{
	return m_ControllerInst.GetBool(n, out);
}

const _bool CAnimator::GetInt(const wstring& n, _bool& out) const
{
	return m_ControllerInst.GetInt(n, out);
}

const _bool CAnimator::GetFloat(const wstring& n, _float& out) const
{
	return m_ControllerInst.GetFloat(n, out);
}
