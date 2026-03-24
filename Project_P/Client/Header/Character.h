#pragma once
#include "cpch.h"
#include "Component.h"

struct HurtDescription;

class CCharacter abstract : public CComponent
{
	struct ColliderBoxBonesInfo
	{
		wstring boneName = L"";
		vector3 size = vector3::one() * 0.5f;
		vector3 center = vector3::zero();
		CCollider::ColliderType shape = CCollider::ColliderType::Sphere;
	};

public:
	CCharacter();
	~CCharacter();

public:
	HRESULT Initialize() override;
	void Awake() override;
	void Start() override;
	void Update() override;
	void FixedUpdate() override;
	void OnDestroy() override;

public:
	CAnimator* GetAnimator();
	CRigidBody* GetRigidBody();

	void AddHurtBox(const wstring& _name, class CHurtBox* _box);

public:
	const wstring& GetCharacterName() const;

public:
	virtual void GetHitHandler(const HurtDescription& _hurtDesc) PURE;

public:
	void SetAbleNavAgent(const _bool _value);

public:
	const _float GetHeight() const;

protected:
	virtual void SetAnimationAction() PURE;
	void CreateHurtBox();
	void CreateHitBox();

private:
	void Ground();

protected:
	wstring m_strCharacterName;

	CSkinnedMeshRenderer* m_pSkinnedMeshRenderer;

	CCapsuleCollider* m_pBodyCollider;
	CRigidBody* m_pRigidBody;
	CAnimator* m_pAnimator;
	CNaviMeshAgent* m_pNavAgent;

	CSceneManager::LayerMask m_iGroundMask;
	_bool m_bIsGround;

	vector<ColliderBoxBonesInfo> m_vHurtBoxInfoList, m_vHitBoxInfoList;
	map<wstring, class CHurtBox*> m_mHurtBoxList;
};

