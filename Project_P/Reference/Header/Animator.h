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

    const _bool IsPlaying() const;

    void Play();
    void Play(const wstring& _animName, const _float _blendDuration = 0.f, const _bool _restartSame = false);
    void Pause();
    void Stop();
    void PlayBlendTree(const CAnimatorController::State::BlendTree& _blendTree, const _float _blendDuration = 0.f, const _bool _restartSame = false);

public:
    void Set_Controller(CAnimatorController* _controller, const _bool _playEntry = true);

public:
    void SetBool(const wstring& n, _bool v);
    void SetInt(const wstring& n, _int v);
    void SetFloat(const wstring& n, _float v);
    void SetTrigger(const wstring& n);
    void ResetTrigger(const wstring& n);
    void RegisterActionHandler(const wstring& name, const function<void()>& handler);
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
    struct DirectBlendState;
    _bool IsRootBone(const wstring& _name);
    void ApplyRootMotionDelta(const vector3& rootPos);
    void ProcessActionTriggers(CAnimationClip* clip, _float prevTime, _float currentTime);
    void ResetActionTriggerState();
    _float GetParamValue(const wstring& name) const;
    void UpdateDirectBlendState(struct DirectBlendState& state, const CAnimatorController::State::BlendTree* tree, _float dt);
    _float GetDirectBlendParamValue(const CAnimatorController::State::BlendTree& tree) const;
    void ComputeBlendTreeWeights(const CAnimatorController::State::BlendTree& tree, vector<_float>& weights, vector<const CAnimatorController::State::BlendTreeChild*>& children) const;
    _float GetBlendTreeDuration(const CAnimatorController::State::BlendTree& tree) const;
    _bool IsBlendTreeLoop(const CAnimatorController::State::BlendTree& tree) const;
    CAnimationClip* GetBlendTreeDominantClip(const CAnimatorController::State::BlendTree& tree) const;
    void SampleBlendTreePose(const CAnimatorController::State::BlendTree& tree, _float time, unordered_map<wstring, CAnimationClip::BoneTransform>& outPose, vector3* outRootPos);

private:
    class CSkinnedMeshRenderer* m_pSkinnedRenderer;
    unordered_map<wstring, CAnimationClip*> m_mAnimationList;
    CAnimationClip* m_pCrtAnimation, * m_pNextAnimation;
    _bool m_bApplyRootMotion;
    _bool m_bIsPlaying, m_bBlending, m_bLoop;
    _float m_fCurrentTime, m_fBlendTime, m_fBlendDuration, m_fNextTime;
    _int m_iPrevTriggerFrame;
    CAnimationClip* m_pPrevTriggerClip;
    unordered_map<wstring, function<void()>> m_mActionHandlers;
    _float m_fPlaybackSpeed;
    vector<_matrix> m_vFinalBoneMatrix;
	AnimatorStateInfo m_sStateInfo;
    const CAnimatorController::State::BlendTree* m_pBlendTree;
    const CAnimatorController::State::BlendTree* m_pNextBlendTree;
    _bool m_bBlendTreeActive;
    _bool m_bNextBlendTreeActive;

    unordered_map<wstring, CAnimationClip::BoneTransform> m_mBlendStartPose;

    struct DirectBlendState
    {
        const CAnimatorController::State::BlendTree* tree = nullptr;
        _float current = 0.f;
        _float target = 0.f;
        _float start = 0.f;
        _float timer = 0.f;
        _float duration = 0.f;
        _bool active = false;
        _bool hasValue = false;
    };
    DirectBlendState m_directBlendCurrent;
    DirectBlendState m_directBlendNext;

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
