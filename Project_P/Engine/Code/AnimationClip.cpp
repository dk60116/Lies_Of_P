#include "epch.h"
#include "AnimationClip.h"

CAnimationClip::CAnimationClip()
	: m_vBoneAnimation({})
	, m_vActionTriggerList({})
	, m_bLoopTime(false)
	, m_fSpeed(1.f)
	, m_fDuration(0.f)
	, m_fTicksPerSecond(0.f)
{
	m_strName = L"Animation";
}

CAnimationClip::~CAnimationClip()
{
	OnDestroy();
}

CAnimationClip* CAnimationClip::Create()
{
	return new CAnimationClip();
}

void CAnimationClip::OnDestroy()
{
	__super::OnDestroy();

	m_vBoneAnimation.clear();
	m_vActionTriggerList.clear();
}

HRESULT CAnimationClip::Initialize(const wstring& _name, const wstring& _filePath, void* _desc)
{
	if (FAILED(__super::Initialize(_name, _filePath, _desc)))
		return E_FAIL;

	return S_OK;
}

HRESULT CAnimationClip::Initiailize_Custom(AnimationClipInitInfo _info, void* _desc)
{
	m_fDuration = _info.duration;
	m_fTicksPerSecond = _info.ticksPerSecond;
	m_bLoopTime = _info.loop;
	m_fSpeed = _info.speed;

	m_vBoneAnimation = {};

	for (_uint i = 0; i < _info.tracks.size(); ++i)
		m_vBoneAnimation.push_back(_info.tracks[i]);

	return S_OK;
}

_int CAnimationClip::Sample(_float _timeSec, unordered_map<wstring, BoneTransform>& _out) const
{
	_int frameIndex = -1;

	if (m_vBoneAnimation.empty() || m_fDuration == 0.f)
		return frameIndex;

	double ticks = _timeSec * m_fTicksPerSecond * m_fSpeed;
	double time = ticks;
	if (m_bLoopTime)
	{
		time = fmod(ticks, m_fDuration);

		if (time < 0.0)
			time += m_fDuration;

		if (ticks > 0.0 && fabs(time) <= 1e-8)
			time = m_fDuration;
	}
	else
	{
		if (time < 0.0)
			time = 0.0;
		if (time > m_fDuration)
			time = m_fDuration;
	}

	_out.clear();
	_out.reserve(m_vBoneAnimation.size());

	for (const auto& ba : m_vBoneAnimation)
	{
		const auto& keys = ba.keyframes;
		if (keys.empty())
			continue;

		size_t i1 = 0, i2 = 0;
		while (i2 < keys.size() && time >= keys[i2].timeStamp) { i1 = i2++; }

		if (i2 >= keys.size())
			i2 = i1;

		const _float span = float(keys[i2].timeStamp - keys[i1].timeStamp);
		const _float t = span > 0.f ? clamp(float((time - keys[i1].timeStamp) / span), 0.f, 1.f) : 0.f;

		BoneTransform bt;
		XMStoreFloat3
		(
			&bt.pos,
			XMVectorLerp(XMLoadFloat3(&keys[i1].position),
				XMLoadFloat3(&keys[i2].position), t)
		);

		XMStoreFloat4(&bt.rot, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&keys[i1].rotation), XMLoadFloat4(&keys[i2].rotation), t)));

		XMStoreFloat3(&bt.scale, XMVectorLerp(XMLoadFloat3(&keys[i1].scaling), XMLoadFloat3(&keys[i2].scaling), t));

		_out.emplace(ba.nodeName, bt);

		frameIndex = static_cast<_int>(i1);
	}

	return frameIndex;
}

void CAnimationClip::BuildBoneToTrackMap(const vector<wstring>& boneNames, vector<_int>& outBoneToTrack) const
{
	outBoneToTrack.assign(boneNames.size(), -1);

	for (_uint b = 0; b < (_uint)boneNames.size(); ++b)
	{
		for (_uint t = 0; t < (_uint)m_vBoneAnimation.size(); ++t)
		{
			if (m_vBoneAnimation[t].nodeName == boneNames[b])
			{
				outBoneToTrack[b] = (_int)t;
				break;
			}
		}
	}
}

_int CAnimationClip::SampleIndexed(_float _timeSec, const vector<_int>& boneToTrack, vector<BoneTransform>& out) const
{
	_int frameIndex = -1;

	out.assign(boneToTrack.size(), BoneTransform{});

	if (m_vBoneAnimation.empty() || m_fDuration == 0.f)
		return frameIndex;

	double ticks = _timeSec * m_fTicksPerSecond * m_fSpeed;
	double time = ticks;
	if (m_bLoopTime)
	{
		time = fmod(ticks, m_fDuration);
		if (time < 0.0) time += m_fDuration;
		if (ticks > 0.0 && fabs(time) <= 1e-8) time = m_fDuration;
	}
	else
	{
		if (time < 0.0) time = 0.0;
		if (time > m_fDuration) time = m_fDuration;
	}

	for (_uint b = 0; b < (_uint)boneToTrack.size(); ++b)
	{
		const _int trackIdx = boneToTrack[b];
		if (trackIdx < 0 || trackIdx >= (_int)m_vBoneAnimation.size())
			continue;

		const auto& ba = m_vBoneAnimation[trackIdx];
		const auto& keys = ba.keyframes;
		if (keys.empty())
			continue;

		size_t i1 = 0, i2 = 0;
		while (i2 < keys.size() && time >= keys[i2].timeStamp) { i1 = i2++; }
		if (i2 >= keys.size()) i2 = i1;

		const _float span = float(keys[i2].timeStamp - keys[i1].timeStamp);
		const _float t = span > 0.f ? clamp(float((time - keys[i1].timeStamp) / span), 0.f, 1.f) : 0.f;

		BoneTransform& bt = out[b];
		XMStoreFloat3(&bt.pos, XMVectorLerp(XMLoadFloat3(&keys[i1].position), XMLoadFloat3(&keys[i2].position), t));
		XMStoreFloat4(&bt.rot, XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&keys[i1].rotation), XMLoadFloat4(&keys[i2].rotation), t)));
		XMStoreFloat3(&bt.scale, XMVectorLerp(XMLoadFloat3(&keys[i1].scaling), XMLoadFloat3(&keys[i2].scaling), t));

		if (frameIndex < 0)
			frameIndex = static_cast<_int>(i1);
	}

	return frameIndex;
}

const _bool CAnimationClip::IsLoop() const
{
	return m_bLoopTime;
}

void CAnimationClip::SetLoop(const _bool _loop)
{
	m_bLoopTime = _loop;
}

const _float CAnimationClip::Get_Speed() const
{
	return m_fSpeed;
}

void CAnimationClip::SetSpeed(const _float _speed)
{
	m_fSpeed = _speed;
}

const _float CAnimationClip::Get_Duration() const
{
	return m_fDuration / m_fTicksPerSecond;
}

const _float CAnimationClip::Get_TickPerSecons() const
{
	return m_fTicksPerSecond;
}

const vector<CAnimationClip::ActionTrigger>& CAnimationClip::Get_ActionTriggerList() const
{
	return m_vActionTriggerList;
}

void CAnimationClip::Add_ActionTrigger(const ActionTrigger& _trigger)
{
	m_vActionTriggerList.push_back(_trigger);
}

const _bool CAnimationClip::Remove_ActionTrigger(const ActionTrigger& _trigger)
{
	auto iter = std::find_if(m_vActionTriggerList.begin(), m_vActionTriggerList.end(),
		[&_trigger](const ActionTrigger& item)
		{
			return item.frame == _trigger.frame && item.actionName == _trigger.actionName;
		});

	if (iter == m_vActionTriggerList.end())
		return false;

	m_vActionTriggerList.erase(iter);
	return true;
}

const _uint CAnimationClip::Get_FrameCount() const
{
	size_t maxCount = 0;

	for (const auto& ba : m_vBoneAnimation)
		maxCount = max(maxCount, ba.keyframes.size());

	return static_cast<_int>(maxCount);
}

const _uint CAnimationClip::Get_LastFrameIndex() const
{
	_uint count = Get_FrameCount();
	return (count > 0) ? (count - 1) : -1;
}

const _uint CAnimationClip::Get_NormalizedFrameIndex(_float _value)
{
	const _int frameCount = static_cast<_int>(Get_FrameCount());

	if (frameCount <= 0)
		return -1;

	_value = clamp(_value, 0.f, 1.f);

	const _float f = _value * (_float)(frameCount - 1);
	_int idx = (_int)lround((double)f);

	idx = clamp(idx, 0, frameCount - 1);
	return static_cast<_uint>(idx);
}
