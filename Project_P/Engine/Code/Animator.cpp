#include "epch.h"
#include "Animator.h"
#include "SkinnedMeshRenderer.h"

CAnimator::CAnimator()
	: m_pSkinnedRenderer(nullptr)
	, m_mAnimationList({})
	, m_pCrtAnimation(nullptr)
	, m_pNextAnimation(nullptr)
	, m_bIsPlaying(false)
	, m_bBlending(false)
	, m_bLoop(false)
	, m_fCurrentTime(0.f)
	, m_fBlendTime(0.f)
	, m_fBlendDuration(0.f)
	, m_fPlaybackSpeed(1.f)
	, m_vFinalBoneMatrix({})
	, m_mBlendStartPose({})
	, m_sStateInfo({})
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
	clone->m_bIsPlaying = this->m_bIsPlaying;
	clone->m_fCurrentTime = this->m_fCurrentTime;
	clone->m_fBlendTime = this->m_fBlendTime;
	clone->m_fBlendDuration = this->m_fBlendDuration;
	clone->m_fPlaybackSpeed = this->m_fPlaybackSpeed;
	clone->m_vFinalBoneMatrix = this->m_vFinalBoneMatrix;
	clone->m_mBlendStartPose = this->m_mBlendStartPose;
	clone->m_sStateInfo = this->m_sStateInfo;

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
		m_pSkinnedRenderer = m_pGameObject->Get_Transform()->Get_Child(0)->Get_GameObject()->GetComponent<CSkinnedMeshRenderer>();

		if (m_pSkinnedRenderer)
			m_pSkinnedRenderer->AddRef();
	}

	return S_OK;
}

void CAnimator::Awake()
{
}

void CAnimator::Update()
{
	if (!m_bIsPlaying || !m_pSkinnedRenderer || !m_pCrtAnimation)
		return;

	m_fCurrentTime += DELTA_TIME * m_fPlaybackSpeed;

	if (m_bBlending)
	{
		m_fBlendTime += DELTA_TIME;

		_float t = m_fBlendTime / m_fBlendDuration;

		if (t >= 1.f)
		{
			// 블렌딩 완료
			m_pCrtAnimation = m_pNextAnimation;
			m_pNextAnimation = nullptr;
			m_fCurrentTime = 0.f;
			m_bBlending = false;
			t = 1.f;
			return;
		}

		// 현재/다음 애니메이션 각각 샘플링
		unordered_map<wstring, CAnimationClip::BoneTransform> sampledNext;
		m_pNextAnimation->Sample(0.f, sampledNext);

		const _uint boneCount = m_pSkinnedRenderer->Get_BoneCount();
		for (_uint i = 0; i < boneCount; ++i)
		{
			CTransform* bone = m_pSkinnedRenderer->Get_BoneTransform(i);
			const wstring& name = m_pSkinnedRenderer->Get_BoneName(i);

			if (!bone)
				continue;

			if (!m_pSkinnedRenderer->m_bApplyRootMotion && name == m_pSkinnedRenderer->Get_RootBoneName())
				continue;

			if (bone->Get_GameObject()->Get_ObjectName() == L"root")
				continue;

			const auto& startIt = m_mBlendStartPose.find(name);
			const auto& nextIt = sampledNext.find(name);

			if (startIt != m_mBlendStartPose.end() && nextIt != sampledNext.end())
			{
				const auto& btStart = startIt->second;
				const auto& btNext = nextIt->second;

				// 선형 보간 (Lerp)
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

	const _float duration = m_pCrtAnimation->Get_Duration();
	m_sStateInfo.length = duration;

	if (m_bLoop)
		m_fCurrentTime = fmodf(m_fCurrentTime, duration);
	else if (m_fCurrentTime >= duration)
	{
		m_fCurrentTime = duration;
		m_bIsPlaying = false;
	}

	// 현재 시각의 키프레임 샘플링
	unordered_map<wstring, CAnimationClip::BoneTransform> sampled;
	m_pCrtAnimation->Sample(m_fCurrentTime, sampled);

	// 각 본 CTransform 갱신
	const _uint boneCount = m_pSkinnedRenderer->Get_BoneCount();

	for (_uint i = 0; i < boneCount; ++i)
	{
		CTransform* bone = m_pSkinnedRenderer->Get_BoneTransform(i);
		if (!bone)
			continue;

		const wstring& name = m_pSkinnedRenderer->Get_BoneName(i);

		if (!m_pSkinnedRenderer->m_bApplyRootMotion && name == m_pSkinnedRenderer->Get_RootBoneName())
			continue;

		if (bone->Get_GameObject()->Get_ObjectName() == L"root")
			continue;

		auto it = sampled.find(name);
		
		if (it == sampled.end())
			continue;

		const auto& bt = it->second;

		bone->Set_LocalPosition(bt.pos);
		bone->Set_LocalQuaternion(bt.rot);
		bone->Set_LocalScale(bt.scale);
	}

	m_sStateInfo.normalizeTime = m_fCurrentTime / duration;
}

void CAnimator::OnDestroy()
{
	for (TRAVERSAL_ITER(m_mAnimationList, it))
		Safe_Release((*it).second);

	m_mAnimationList.clear();

	Safe_Release(m_pSkinnedRenderer);
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

void CAnimator::Set_PlaybackSpeed(const _float _value)
{
	m_fPlaybackSpeed = _value;
}

void CAnimator::Play()
{
	if (!m_pSkinnedRenderer)
	{
		CDebug::LogError(L"Animator play failed - Skinned renderer is nullptr: " + m_pGameObject->Get_ObjectNameID());
		return;
	}

	if (m_pCrtAnimation)
		m_bIsPlaying = true;
}

void CAnimator::Play(const wstring& _animName, const _float _blendDuration)
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

	if (_blendDuration <= 0.f || !m_pCrtAnimation)
	{
		m_pCrtAnimation = nextAnim;
		m_pNextAnimation = nullptr;
		m_fCurrentTime = 0.f;
		m_bIsPlaying = true;
		m_bBlending = false;
		return;
	}

	m_pNextAnimation = nextAnim;
	m_fBlendTime = 0.f;
	m_fBlendDuration = _blendDuration;
	m_bBlending = true;

	if (!m_bIsPlaying)
		m_fCurrentTime = 0.f;

	m_mBlendStartPose.clear();
	m_pCrtAnimation->Sample(m_fCurrentTime, m_mBlendStartPose);

	m_bIsPlaying = true;
}

void CAnimator::Pause()
{
	m_bIsPlaying = false;
}

void CAnimator::Stop()
{
	m_fCurrentTime = 0.f;
	Update();
	m_bIsPlaying = false;
}

void CAnimator::SetLoop(const _bool _loop)
{
	m_bLoop = _loop;
}

void CAnimator::SetSpeed(const _float _value)
{
	m_fPlaybackSpeed = _value;
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
