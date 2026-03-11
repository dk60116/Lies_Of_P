#include "cpch.h"
#include "HurtBox.h"

CHurtBox::CHurtBox()
{
}

CHurtBox::~CHurtBox()
{
}

void CHurtBox::OnTriggerEnter(CCollider* _other)
{
    CDebug::LogError("Call");
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
    m_pCharacter->AddHurtBox(m_strCBName, this);
}
