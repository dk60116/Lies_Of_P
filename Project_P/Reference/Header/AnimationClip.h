#pragma once

#include "EngineResource.h"

NS_BEGIN(Engine)

class ENGINE_DLL CAnimationClip : public CEngineResource
{
	friend class CResources;

public:
	struct Keyframe
	{
		double timeStamp = {};
		_float3 position = {};
		_float4 rotation = {};
		_float3 scaling = {};
	};

	struct BoneTransform
	{
		_float3 pos = { 0.f , 0.f, 0.f };
		_float4 rot = { 0.f, 0.f, 0.f, 1.f };
		_float3 scale = { 1.f ,1.f, 1.f };
	};

	struct NodeTrack
	{
		wstring nodeName = L"";
		vector<Keyframe> keyframes = {};
	};

	struct AnimationClipInitInfo
	{
		wstring name = L"";
		_float duration = 0.f;
		_float ticksPerSecond = 25.f;
		_bool loop = false;
		_float speed = 1.f;

		vector<NodeTrack> tracks;
	};

	struct ActionTrigger
	{
		_uint frame = 0;
		wstring actionName = L"";
	};

protected:
	CAnimationClip();
	~CAnimationClip();

protected:
	static CAnimationClip* Create();
	HRESULT Initialize(const wstring& _name, const wstring& _filePath, void* _desc) override;
	void OnDestroy() override;

public:
	HRESULT Initiailize_Custom(AnimationClipInitInfo _info, void* _desc);

public:
	_int Sample(_float _timeSec, unordered_map<wstring, BoneTransform>& _out) const;

	void BuildBoneToTrackMap(const vector<wstring>& boneNames, vector<_int>& outBoneToTrack) const;
	_int SampleIndexed(_float _timeSec, const vector<_int>& boneToTrack, vector<BoneTransform>& out) const;

	const _bool IsLoop() const;
	void SetLoop(const _bool _loop);
	const _float Get_Speed() const;
	void SetSpeed(const _float _speed);
	const _float Get_Duration() const;
	const _float Get_TickPerSecons() const;
	const vector<ActionTrigger>& Get_ActionTriggerList() const;
	void Add_ActionTrigger(const ActionTrigger& _trigger);
	const _bool Remove_ActionTrigger(const ActionTrigger& _trigger);
	const _uint Get_FrameCount() const;
	const _uint Get_LastFrameIndex() const;
	const _uint Get_NormalizedFrameIndex(_float _value);

private:
	vector<NodeTrack> m_vBoneAnimation;
	vector<ActionTrigger> m_vActionTriggerList;
	_bool m_bLoopTime;
	_float m_fSpeed;
	_float m_fDuration;
	_float m_fTicksPerSecond;
};

NS_END
