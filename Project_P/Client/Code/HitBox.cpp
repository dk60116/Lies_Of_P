#include "cpch.h"
#include "HitBox.h"

CHitBox::CHitBox()
{
}

CHitBox::~CHitBox()
{
}

CHitBox* CHitBox::Create()
{
	return new CHitBox();
}

CComponent* CHitBox::Clone() const
{
	CHitBox* clone = new CHitBox();

	return clone;
}
