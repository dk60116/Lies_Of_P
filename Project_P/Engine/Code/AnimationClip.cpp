#include "epch.h"
#include "AnimationClip.h"

CAnimationClip::CAnimationClip()
	: m_vBoneAnimation({})
	, m_vActionTriggerList({})
	, m_bLoopTime(false)
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

	double ticks = _timeSec * m_fTicksPerSecond;
	double time = fmod(ticks, m_fDuration);

	_out.clear();
	_out.reserve(m_vBoneAnimation.size());

	for (const auto& ba : m_vBoneAnimation)
	{
		const auto& keys = ba.keyframes;
		if (keys.empty())
			continue;

		size_t i1 = 0, i2 = 0;
		while (i2 < keys.size() && time >= keys[i2].timeStamp) { i1 = i2++; }

		if (i2 >= keys.size()) { i2 = i1; }          // ³¡ ±¸°£
		_float span = float(keys[i2].timeStamp - keys[i1].timeStamp);
		_float  t = span > 0.f ? float((time - keys[i1].timeStamp) / span) : 0.f;

		// º¸°£
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

const _bool CAnimationClip::IsLoop() const
{
	return m_bLoopTime;
}

void CAnimationClip::SetLoop(const _bool _loop)
{
	m_bLoopTime = _loop;
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

_bool CAnimationClip::Remove_ActionTrigger(const ActionTrigger& _trigger)
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
