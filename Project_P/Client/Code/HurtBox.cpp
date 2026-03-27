#include "cpch.h"
#include "HurtBox.h"
#include "HitBox.h"

namespace
{
    size_t GetHitTargetKey(CCharacter* _target)
    {
        if (!_target)
            return 0;

        if (CGameObject* targetObject = _target->Get_GameObject())
            return static_cast<size_t>(targetObject->Get_UniqueID());

        return reinterpret_cast<size_t>(_target);
    }
}

CHurtBox::CHurtBox()
    :m_sHurtDesc({})
    , m_pRigid(nullptr)
    , m_setHitTargets({})
{
}

CHurtBox::~CHurtBox()
{
}

CHurtBox* CHurtBox::Create()
{
    return new CHurtBox();
}

CComponent* CHurtBox::Clone() const
{
    CHurtBox* clone = new CHurtBox();

    return clone;
}

HRESULT CHurtBox::Initialize()
{
    if (FAILED(__super::Initialize()))
        return E_FAIL;

    m_pRigid = m_pGameObject->AddComponent<CRigidBody>();
    m_pRigid->SetUseGravity(false);
    m_pRigid->SetKinematic(true);

    return S_OK;
}

void CHurtBox::Awake()
{
    __super::Awake();

    m_pCharacter->AddHurtBox(m_strCBName, this);
}

void CHurtBox::OnTriggerEnter(CCollider* _other)
{
    HandleHitOverlap(_other);
}

void CHurtBox::OnTriggerStay(CCollider* _other)
{
    HandleHitOverlap(_other);
}

void CHurtBox::SetDamage(const _int damage)
{
    m_sHurtDesc.damage = max(0, damage);
}

const _int CHurtBox::GetDamage() const
{
    return m_sHurtDesc.damage;
}

void CHurtBox::SetKnockback(const _bool _knockback)
{
    m_sHurtDesc.knockback = _knockback;
    if (!_knockback)
        m_sHurtDesc.knockbackAmount = 0.f;
    else if (m_sHurtDesc.knockbackAmount <= 0.f)
        m_sHurtDesc.knockbackAmount = 1.f;
}

void CHurtBox::SetKnockbackAmount(const _float _knockbackAmount)
{
    m_sHurtDesc.knockbackAmount = max(0.f, _knockbackAmount);
    m_sHurtDesc.knockback = m_sHurtDesc.knockbackAmount > 0.f;
}

void CHurtBox::HandleHitOverlap(CCollider* _other)
{
    if (!_other || !_other->Get_GameObject())
        return;

    CCharacter* target = nullptr;

    if (m_eOwner == ColliderOwner::Player)
    {
        if (!_other->Get_GameObject()->CompareTag(L"MonsterBody"))
            return;

        if (CHitBox* hitBox = _other->Get_GameObject()->GetComponent<CHitBox>())
            target = hitBox->GetCharacter();
    }
    else if (m_eOwner == ColliderOwner::Enemy)
    {
        if (!_other->Get_GameObject()->CompareTag(L"PlayerBody"))
            return;

        target = _other->Get_GameObject()->GetComponent<CPlayer>();
    }
    else
    {
        return;
    }

    if (!TryRegisterHitTarget(target))
        return;

    OnHitEvent(target);

    if (m_eOwner == ColliderOwner::Enemy)
        RequestDisableBox();
}

_bool CHurtBox::TryRegisterHitTarget(CCharacter* _target)
{
    if (!_target)
        return false;

    const size_t targetKey = GetHitTargetKey(_target);
    if (targetKey == 0)
        return true;

    const auto [it, inserted] = m_setHitTargets.insert(targetKey);
    return inserted;
}

void CHurtBox::OnHitEvent(CCharacter* _target)
{
    if (!_target)
        return;

    CTransform* attackerTransform = m_pCharacter ? m_pCharacter->GetTransform() : GetTransform();
    m_sHurtDesc.position = attackerTransform ? attackerTransform->Get_Position() : vector3::zero();
    m_sHurtDesc.forward = attackerTransform ? attackerTransform->Get_Directions().forward : vector3::zero();
    _target->GetHitHandler(m_sHurtDesc);
}

void CHurtBox::OnBoxEnabled()
{
    m_setHitTargets.clear();
}

void CHurtBox::OnBoxDisabled()
{
    m_setHitTargets.clear();
}


