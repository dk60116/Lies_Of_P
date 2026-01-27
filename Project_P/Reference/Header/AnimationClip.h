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

		vector<NodeTrack> tracks;
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
	const _bool IsLoop() const;
	const _float Get_Duration() const;
	const _float Get_TickPerSecons() const;

private:
	vector<NodeTrack> m_vBoneAnimation;
	_bool m_bLoopTime;
	_float m_fDuration;
	_float m_fTicksPerSecond;
};

NS_END