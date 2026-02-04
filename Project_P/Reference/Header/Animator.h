#pragma once

#include "Component.h"
#include "AnimatorController.h"
#include <functional>

NS_BEGIN(Engine)

class ENGINE_DLL CAnimator final : public CComponent
{
    friend class CGameObject;

public:
    struct AnimatorStateInfo
    {
        _float length;
        _float normalizeTime;
    };

protected:
 	explicit CAnimator();
    ~CAnimator();

private:
    static CAnimator* Create();
    CComponent* Clone() const override;

public:
    HRESULT Initialize() override;
    void Awake() override;
    void Update() override;
    void OnDestroy() override;

public:
    const _bool IsLoop() const;
    const _bool ApplyRootmotion() const;

    void SetApplyRootmotion(const _bool _value, CTransform* _target);
    void Set_PlaybackSpeed(const _float _value);

    void Add_Animation(const wstring& _animName, CAnimationClip* _anim);

    void Play();
    void Play(const wstring& _animName, const _float _blendDuration = 0.f, const _bool _restartSame = false);
    void Pause();
    void Stop();

public:
    void Set_Controller(CAnimatorController* _controller, const _bool _playEntry = true);

public:
    void SetBool(const wstring& n, _bool v);
    void SetInt(const wstring& n, _int v);
    void SetFloat(const wstring& n, _float v);
    void SetTrigger(const wstring& n);
    void ResetTrigger(const wstring& n);
    void RegisterActionHandler(const wstring& name, const std::function<void()>& handler);
    void UnregisterActionHandler(const wstring& name);
    void ClearActionHandlers();

    const _bool GetBool(const wstring& n, _bool& out) const;
    const _bool GetInt(const wstring& n, _bool& out) const;
    const _bool GetFloat(const wstring& n, _float& out) const;

public:
    unordered_map<wstring, CAnimationClip*>& Get_AnimationClipList();
    CAnimationClip* Get_CurrentAnimation();
    AnimatorStateInfo& Get_StateInfo();

public:
    const _float GetNormalizedTime() const;

private:
    _bool IsRootBone(const wstring& _name);
    void ApplyRootMotionDelta(const vector3& rootPos);
    void ProcessActionTriggers(CAnimationClip* clip, _float prevTime, _float currentTime);
    void ResetActionTriggerState();

private:
    class CSkinnedMeshRenderer* m_pSkinnedRenderer;
    unordered_map<wstring, CAnimationClip*> m_mAnimationList;
    CAnimationClip* m_pCrtAnimation, * m_pNextAnimation;
    _bool m_bApplyRootMotion;
    _bool m_bIsPlaying, m_bBlending, m_bLoop;
    _float m_fCurrentTime, m_fBlendTime, m_fBlendDuration, m_fNextTime;
    _int m_iPrevTriggerFrame;
    CAnimationClip* m_pPrevTriggerClip;
    unordered_map<wstring, std::function<void()>> m_mActionHandlers;
    _float m_fPlaybackSpeed;
    vector<_matrix> m_vFinalBoneMatrix;
	AnimatorStateInfo m_sStateInfo;

    unordered_map<wstring, CAnimationClip::BoneTransform> m_mBlendStartPose;

    CTransform* m_pRootMotionParent;
    vector3 m_vPrevRootMotionPos;
    _bool m_bHasPrevRootMotion;
    vector3 m_vNextRootStartPos;
    _bool m_bHasNextRootStartPos;

private:
    class CAnimatorController* m_pController;
    CAnimatorControllerInstance m_ControllerInst;

    BEGIN_SERIALIZEFIELD
        SERIALIZEFIELD(m_pSkinnedRenderer)
        SERIALIZEFIELD(m_pCrtAnimation)
    END_SERIALIZEFIELD
};

NS_END
