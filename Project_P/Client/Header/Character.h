#pragma once
#include "Component.h"

class CCharacter abstract : public CComponent
{
	struct ColliderBoxBonesInfo
	{
		wstring boneName = L"";
		vector3 size = vector3::one() * 0.5f;
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

protected:
	virtual void SetAnimationAction() PURE;
	void CreateHurtBox();

private:
	void Ground();

protected:
	CSkinnedMeshRenderer* m_pSkinnedMeshRenderer;

	CCapsuleCollider* m_pBodyCollider;
	CRigidBody* m_pRigidBody;
	CAnimator* m_pAnimator;

	CSceneManager::LayerMask m_iGroundMask;
	_bool m_bIsGround;

	vector<ColliderBoxBonesInfo> m_vHurtBoxInfoList;
	map<wstring, class CHurtBox*> m_mHurtBoxList;
};

