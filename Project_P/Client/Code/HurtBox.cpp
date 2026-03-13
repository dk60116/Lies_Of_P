#include "cpch.h"
#include "HurtBox.h"
#include "HitBox.h"

CHurtBox::CHurtBox()
    :m_sHurtDesc({})
    , m_pRigid(nullptr)
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
    if (m_eOwner == ColliderOwner::Player)
    {
        if (_other->Get_GameObject()->CompareTag(L"MonsterBody"))
        {
            CCharacter* ch = _other->Get_GameObject()->GetComponent<CHitBox>()->GetCharacter();
            OnHitEvent(ch);
        }
    }
    else if (m_eOwner == ColliderOwner::Enemy)
    {
        if (_other->Get_GameObject()->CompareTag(L"PlayerBody"))
        {
            OnHitEvent(_other->Get_GameObject()->GetComponent<CPlayer>());
        }
    }
}

void CHurtBox::OnTriggerStay(CCollider* _other)
{
    OnTriggerEnter(_other);
}

void CHurtBox::OnHitEvent(CCharacter* _target)
{
    if (!_target)
        return;

    m_sHurtDesc.position = m_pCharacter ? m_pCharacter->Get_Transform()->Get_Position() : Get_Transform()->Get_Position();
    _target->GetHitHandler(m_sHurtDesc);
}



