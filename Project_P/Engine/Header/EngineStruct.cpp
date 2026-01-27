#include "epch.h"

quaternion vector3::to_quaternion() const
{
    _vector vRad = XMVectorSet
    (
        XMConvertToRadians(x),
        XMConvertToRadians(y),
        XMConvertToRadians(z),
        0.f
    );

    return quaternion(XMQuaternionRotationRollPitchYawFromVector(vRad));
}
