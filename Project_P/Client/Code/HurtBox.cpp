#include "cpch.h"
#include "HurtBox.h"

CHurtBox::CHurtBox()
    :m_sHurtDesc({})
{
}

CHurtBox::~CHurtBox()
{
}

void CHurtBox::OnTriggerEnter(CCollider* _other)
{
    if (_other->Get_GameObject()->CompareTag(L"PlayerBody"))
    {
        OnHitEvent(_other->Get_GameObject()->GetComponent<CPlayer>());
    }
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

void CHurtBox::Awake()
{
    __super::Awake();

    m_pCharacter->AddHurtBox(m_strCBName, this);
}


void CHurtBox::OnHitEvent(CCharacter* _target)
{
    if (!_target)
        return;

    _target->GetHitHandler(m_sHurtDesc);

    RequestDisableBox();
}
