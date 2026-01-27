#ifndef Engine_Struct_h__
#define Engine_Struct_h__

#include "EngineTypedef.h"
#include "Material.h"
#include "Texture.h"

namespace Engine
{
#pragma region Serialize
    enum class FieldType
    {
        BOOL, INT, FLOAT, FLOAT_RX, FLOAT_RY, FLOAT_RZ, VECTOR3, VECTOR3INT, VECTOR2, VECTOR2INT, STRING, POINTER, COLOR
    };

    struct FieldInfo
    {
        wstring name;
        FieldType type;
        void* ptr;
        wstring filePath;
    };
#pragma endregion

#pragma region vector2
    struct vector2
    {
        _float x;
        _float y;

        vector2()
            : x(0.f), y(0.f)
        {
        }

        vector2(_float _x, _float _y)
            : x(_x), y(_y)
        {
        }

        vector2(_int _x, _int _y)
            : x(static_cast<_float>(_x)), y(static_cast<_float>(_y))
        {
        }

        vector2(const XMFLOAT2& v)
            : x(v.x), y(v.y)
        {
        }

        const _float2& toFloat2()
        {
            return XMFLOAT2(x, y);
        }

        _vector& toXMVector()
        {
            _vector result = XMVectorSet(x, y, 0.f, 0.f);
            return result;
        }

        vector2& operator=(const XMFLOAT2& rhs)
        {
            x = rhs.x;
            y = rhs.y;
            return *this;
        }

        vector2& operator+=(const vector2& rhs)
        {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        vector2& operator-=(const vector2& rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            return *this;
        }

        vector2& operator*=(float scalar)
        {
            x *= scalar;
            y *= scalar;
            return *this;
        }

        vector2& operator/=(float scalar)
        {
            x /= scalar;
            y /= scalar;
            return *this;
        }

        bool operator==(const vector2& rhs) const
        {
            return x == rhs.x && y == rhs.y;
        }

        bool operator!=(const vector2& rhs) const
        {
            return !(*this == rhs);
        }

        float length() const
        {
            XMVECTOR v = XMVectorSet(x, y, 0.f, 0.f);
            XMVECTOR len = XMVector2Length(v);
            return XMVectorGetX(len);
        }

        float lengthSq() const
        {
            XMVECTOR v = XMVectorSet(x, y, 0.f, 0.f);
            XMVECTOR lenSq = XMVector2LengthSq(v);
            return XMVectorGetX(lenSq);
        }

        vector2 normalized() const
        {
            XMVECTOR v = XMVectorSet(x, y, 0.f, 0.f);
            XMVECTOR result = XMVector2Normalize(v);
            return vector2(XMVectorGetX(result), XMVectorGetY(result));
        }

        float dot(const vector2& other) const
        {
            XMFLOAT2 a(x, y);
            XMFLOAT2 b(other.x, other.y);

            XMVECTOR va = XMLoadFloat2(&a);
            XMVECTOR vb = XMLoadFloat2(&b);

            XMVECTOR result = XMVector2Dot(va, vb);
            return XMVectorGetX(result);
        }

        static vector2 zero()
        {
            return vector2(0.f, 0.f);
        }

        static vector2 one()
        {
            return vector2(1.f, 1.f);
        }

        static vector2 right()
        {
            return vector2(1.f, 0.f);
        }

        static vector2 left()
        {
            return vector2(-1.f, 0.f);
        }

        static vector2 up()
        {
            return vector2(0.f, 1.f);
        }

        static vector2 down()
        {
            return vector2(0.f, -1.f);
        }

        static float Distance(const vector2& a, const vector2& b)
        {
            return vector2(b.x - a.x, b.y - a.y).length();
        }

        operator XMFLOAT2() const
        {
            return XMFLOAT2(x, y);
        }

        XMFLOAT2 vector() const
        {
            return XMFLOAT2(x, y);
        }
    };

    // 연산자 오버로딩
    inline vector2 operator+(const vector2& lhs, const vector2& rhs)
    {
        return vector2(lhs.x + rhs.x, lhs.y + rhs.y);
    }

    inline vector2 operator-(const vector2& lhs, const vector2& rhs)
    {
        return vector2(lhs.x - rhs.x, lhs.y - rhs.y);
    }

    inline vector2 operator*(const vector2& vec, float scalar)
    {
        return vector2(vec.x * scalar, vec.y * scalar);
    }

    inline vector2 operator*(float scalar, const vector2& vec)
    {
        return vector2(vec.x * scalar, vec.y * scalar);
    }

    inline vector2 operator/(const vector2& vec, float scalar)
    {
        return vector2(vec.x / scalar, vec.y / scalar);
    }

    inline vector2 operator*(const vector2& lhs, const vector2& rhs)
    {
        return vector2(lhs.x * rhs.x, lhs.y * rhs.y);
    }

    inline vector2 operator/(const vector2& lhs, const vector2& rhs)
    {
        return vector2(lhs.x / rhs.x, lhs.y / rhs.y);
    }

    inline vector2 operator-(const vector2& v)
    {
        return vector2(-v.x, -v.y);
    }
#pragma endregion

#pragma region vector2Int
    struct vector2Int
    {
        int x;
        int y;

        vector2Int()
            : x(0), y(0)
        {
        }

        vector2Int(_int _x, _int _y)
            : x(_x), y(_y)
        {
        }

        vector2Int(_uint _x, _uint _y)
            : x((_int)_x), y((_int)_y)
        {
        }

        vector2Int(_float _x, _float _y)
            : x((_int)_x), y((_int)_y)
        {
        }

        vector2Int(size_t _x, size_t _y)
            : x((_int)_x), y((_int)_y)
        {
        }

        vector2Int(const vector2& v)
            : x(static_cast<int>(v.x)), y(static_cast<int>(v.y))
        {
        }

        const _float2& toFloat2()
        {
            return _float2(static_cast<_float>(x), static_cast<_float>(y));
        }

        _vector& toXMVector()
        {
            _vector result = XMVectorSet(static_cast<_float>(x), static_cast<_float>(y), 0.f, 0.f);
            return result;
        }

        vector2Int& operator=(const vector2Int& rhs)
        {
            x = rhs.x;
            y = rhs.y;
            return *this;
        }

        vector2Int& operator+=(const vector2Int& rhs)
        {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        vector2Int& operator-=(const vector2Int& rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            return *this;
        }

        vector2Int& operator*=(int scalar)
        {
            x *= scalar;
            y *= scalar;
            return *this;
        }

        vector2Int& operator/=(int scalar)
        {
            x /= scalar;
            y /= scalar;
            return *this;
        }

        bool operator==(const vector2Int& rhs) const
        {
            return x == rhs.x && y == rhs.y;
        }

        bool operator!=(const vector2Int& rhs) const
        {
            return !(*this == rhs);
        }

        float length() const
        {
            return sqrtf(static_cast<float>(x * x + y * y));
        }

        int lengthSq() const
        {
            return x * x + y * y;
        }

        vector2 to_vector2() const
        {
            return vector2(static_cast<float>(x), static_cast<float>(y));
        }

        static vector2Int zero()
        {
            return vector2Int(0, 0);
        }

        static vector2Int one()
        {
            return vector2Int(1, 1);
        }

        static vector2Int up()
        {
            return vector2Int(0, 1);
        }

        static vector2Int down()
        {
            return vector2Int(0, -1);
        }

        static vector2Int left()
        {
            return vector2Int(-1, 0);
        }

        static vector2Int right()
        {
            return vector2Int(1, 0);
        }

        static int Distance(const vector2Int& a, const vector2Int& b)
        {
            int dx = b.x - a.x;
            int dy = b.y - a.y;
            return static_cast<int>(sqrtf(static_cast<float>(dx * dx + dy * dy)));
        }

        static int ManhattanDistance(const vector2Int& a, const vector2Int& b)
        {
            return abs(b.x - a.x) + abs(b.y - a.y);
        }
    };

    // 연산자 오버로딩
    inline vector2Int operator+(const vector2Int& lhs, const vector2Int& rhs)
    {
        return vector2Int(lhs.x + rhs.x, lhs.y + rhs.y);
    }

    inline vector2Int operator-(const vector2Int& lhs, const vector2Int& rhs)
    {
        return vector2Int(lhs.x - rhs.x, lhs.y - rhs.y);
    }

    inline vector2Int operator*(const vector2Int& vec, int scalar)
    {
        return vector2Int(vec.x * scalar, vec.y * scalar);
    }

    inline vector2Int operator*(int scalar, const vector2Int& vec)
    {
        return vector2Int(vec.x * scalar, vec.y * scalar);
    }

    inline vector2Int operator/(const vector2Int& vec, int scalar)
    {
        return vector2Int(vec.x / scalar, vec.y / scalar);
    }

    inline vector2Int operator-(const vector2Int& v)
    {
        return vector2Int(-v.x, -v.y);
    }
#pragma endregion

#pragma region vector3
    struct ENGINE_DLL vector3
    {
        float x;
        float y;
        float z;

        vector3()
            : x(0.f), y(0.f), z(0.f)
        {
        }

        vector3(float _x, float _y, float _z)
            : x(_x), y(_y), z(_z)
        {
        }

        vector3(const XMVECTOR& v)
            : x(XMVectorGetX(v)), y(XMVectorGetY(v)), z(XMVectorGetZ(v))
        {
        }

        vector3(const XMFLOAT3& v)
            : x(v.x), y(v.y), z(v.z)
        {
        }

        const _float3 toFloat3()
        {
            return _float3(x, y, z);
        }

        _vector toXMVector()
        {
            _vector result = XMVectorSet(x, y, z, 0.f);

            return result;
        }

        struct quaternion to_quaternion() const;

        vector3& operator=(const vector3& _rhs)
        {
            x = _rhs.x;
            y = _rhs.y;
            z = _rhs.z;
            return *this;
        }

        vector3& operator=(const XMFLOAT3& rhs)
        {
            x = rhs.x;
            y = rhs.y;
            z = rhs.z;
            return *this;
        }

        vector3& operator+=(const vector3& rhs)
        {
            x += rhs.x;
            y += rhs.y;
            z += rhs.z;
            return *this;
        }

        vector3& operator-=(const vector3& rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            z -= rhs.z;
            return *this;
        }

        vector3& operator*=(float scalar)
        {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            return *this;
        }

        vector3& operator/=(float scalar)
        {
            x /= scalar;
            y /= scalar;
            z /= scalar;
            return *this;
        }

        bool operator==(const vector3& rhs) const
        {
            return x == rhs.x && y == rhs.y && z == rhs.z;
        }

        bool operator!=(const vector3& rhs) const
        {
            return !(*this == rhs);
        }

        static const float Distance(const vector3& _a, const vector3& _b)
        {
            return vector3(_b.x - _a.x, _b.y - _a.y, _b.z - _a.z).length();
        }

        static const vector3 Lerp(const vector3& a, const vector3& b, _float t)
        {
            return vector3
            {
                a.x * (1.f - t) + b.x * t,
                a.y * (1.f - t) + b.y * t,
                a.z * (1.f - t) + b.z * t
            };
        }

        static vector3 zero()
        {
            return vector3(0.f, 0.f, 0.f);
        }

        static vector3 one()
        {
            return vector3(1.f, 1.f, 1.f);
        }

        static vector3 up()
        {
            return vector3(0.f, 1.f, 0.f);
        }

        static vector3 down()
        {
            return vector3(0.f, -1.f, 0.f);
        }

        static vector3 left()
        {
            return vector3(-1.f, 0.f, 0.f);
        }

        static vector3 right()
        {
            return vector3(1.f, 0.f, 0.f);
        }

        static vector3 forward()
        {
            return vector3(0.f, 0.f, 1.f);
        }

        static vector3 back()
        {
            return vector3(0.f, 0.f, -1.f);
        }

        float length() const
        {
            _vector v = XMVectorSet(x, y, z, 0.f);
            _vector len = XMVector3Length(v);
            return XMVectorGetX(len);
        }

        float lengthSq() const
        {
            _vector v = XMVectorSet(x, y, z, 0.f);
            _vector len = XMVector3LengthSq(v);
            return XMVectorGetX(len);
        }

        vector3 normalized() const
        {
            _vector v = XMVectorSet(x, y, z, 0.f);
            _vector normal = XMVector3Normalize(v);
            return vector3(XMVectorGetX(normal), XMVectorGetY(normal), XMVectorGetZ(normal));
        }

        float dot(const vector3& other) const
        {
            _vector a = XMVectorSet(x, y, z, 0.f);
            _vector b = XMVectorSet(other.x, other.y, other.z, 0.f);
            _vector dot = XMVector3Dot(a, b);
            return XMVectorGetX(dot);
        }

        static float dot(const vector3& a, const vector3& b)
        {
            _vector va = XMVectorSet(a.x, a.y, a.z, 0.f);
            _vector vb = XMVectorSet(b.x, b.y, b.z, 0.f);
            _vector dot = XMVector3Dot(va, vb);
            return XMVectorGetX(dot);
        }

        vector3 cross(const vector3& other) const
        {
            _vector a = XMVectorSet(x, y, z, 0.f);
            _vector b = XMVectorSet(other.x, other.y, other.z, 0.f);
            _vector cross = XMVector3Cross(a, b);
            return vector3(XMVectorGetX(cross), XMVectorGetY(cross), XMVectorGetZ(cross));
        }

        static vector3 Cross(const vector3& a, const vector3& b)
        {
            _vector va = XMVectorSet(a.x, a.y, a.z, 0.f);
            _vector vb = XMVectorSet(b.x, b.y, b.z, 0.f);
            _vector cross = XMVector3Cross(va, vb);
            return vector3(XMVectorGetX(cross), XMVectorGetY(cross), XMVectorGetZ(cross));
        }

        const _float3 to_float3()
        {
            return _float3(x, y, z);
        }

        const _vector toXMVector() const
        {
            return XMVectorSet(x, y, z, 0.f);
        }

        operator _float3() const
        {
            return _float3(x, y, z);
        }
    };

    inline vector3 operator+(const vector3& lhs, const vector3& rhs)
    {
        return vector3(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
    }

    inline vector3 operator-(const vector3& lhs, const vector3& rhs)
    {
        return vector3(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
    }

    inline vector3 operator*(const vector3& vec, float scalar)
    {
        return vector3(vec.x * scalar, vec.y * scalar, vec.z * scalar);
    }

    inline vector3 operator*(float scalar, const vector3& vec)
    {
        return vector3(vec.x * scalar, vec.y * scalar, vec.z * scalar);
    }

    inline vector3 operator/(const vector3& vec, float scalar)
    {
        return vector3(vec.x / scalar, vec.y / scalar, vec.z / scalar);
    }

    inline vector3 operator*(const vector3& lhs, const vector3& rhs)
    {
        return vector3(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z);
    }

    inline vector3 operator/(const vector3& lhs, const vector3& rhs)
    {
        return vector3(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z);
    }

    inline vector3 operator-(const vector3& v)
    {
        return vector3(-v.x, -v.y, -v.z);
    }
#pragma endregion

#pragma region vector3Int
    struct vector3Int
    {
        int x;
        int y;
        int z;

        vector3Int()
            : x(0), y(0), z(0)
        {
        }

        vector3Int(_int _x, _int _y, _int _z)
            : x(_x), y(_y), z(_z)
        {
        }

        vector3Int(UINT _x, UINT _y, UINT _z)
            : x((_int)_x), y((_int)_y), z((_int)_z)
        {
        }

        vector3Int(const vector3& v)
            : x(static_cast<int>(v.x)), y(static_cast<int>(v.y)), z(static_cast<int>(v.z))
        {
        }

        vector3Int& operator=(const vector3Int& rhs)
        {
            x = rhs.x;
            y = rhs.y;
            z = rhs.z;
            return *this;
        }

        vector3Int& operator+=(const vector3Int& rhs)
        {
            x += rhs.x;
            y += rhs.y;
            z += rhs.z;
            return *this;
        }

        vector3Int& operator-=(const vector3Int& rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            z -= rhs.z;
            return *this;
        }

        vector3Int& operator*=(int scalar)
        {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            return *this;
        }

        vector3Int& operator/=(int scalar)
        {
            x /= scalar;
            y /= scalar;
            z /= scalar;
            return *this;
        }

        bool operator==(const vector3Int& rhs) const
        {
            return x == rhs.x && y == rhs.y && z == rhs.z;
        }

        bool operator!=(const vector3Int& rhs) const
        {
            return !(*this == rhs);
        }

        float length() const
        {
            return sqrtf(static_cast<float>(x * x + y * y + z * z));
        }

        int lengthSq() const
        {
            return x * x + y * y + z * z;
        }

        vector3 to_vector3() const
        {
            return vector3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        }

        static vector3Int zero()
        {
            return vector3Int(0, 0, 0);
        }

        static vector3Int one()
        {
            return vector3Int(1, 1, 1);
        }

        static vector3Int up()
        {
            return vector3Int(0, 1, 0);
        }

        static vector3Int down()
        {
            return vector3Int(0, -1, 0);
        }

        static vector3Int left()
        {
            return vector3Int(-1, 0, 0);
        }

        static vector3Int right()
        {
            return vector3Int(1, 0, 0);
        }

        static vector3Int forward()
        {
            return vector3Int(0, 0, 1);
        }

        static vector3Int back()
        {
            return vector3Int(0, 0, -1);
        }

        static int Distance(const vector3Int& a, const vector3Int& b)
        {
            int dx = b.x - a.x;
            int dy = b.y - a.y;
            int dz = b.z - a.z;
            return static_cast<int>(sqrtf(static_cast<float>(dx * dx + dy * dy + dz * dz)));
        }

        static int ManhattanDistance(const vector3Int& a, const vector3Int& b)
        {
            return abs(b.x - a.x) + abs(b.y - a.y) + abs(b.z - a.z);
        }
    };

    // 연산자 오버로딩
    inline vector3Int operator+(const vector3Int& lhs, const vector3Int& rhs)
    {
        return vector3Int(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
    }

    inline vector3Int operator-(const vector3Int& lhs, const vector3Int& rhs)
    {
        return vector3Int(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
    }

    inline vector3Int operator*(const vector3Int& vec, int scalar)
    {
        return vector3Int(vec.x * scalar, vec.y * scalar, vec.z * scalar);
    }

    inline vector3Int operator*(int scalar, const vector3Int& vec)
    {
        return vector3Int(vec.x * scalar, vec.y * scalar, vec.z * scalar);
    }

    inline vector3Int operator/(const vector3Int& vec, int scalar)
    {
        return vector3Int(vec.x / scalar, vec.y / scalar, vec.z / scalar);
    }

    inline vector3Int operator-(const vector3Int& v)
    {
        return vector3Int(-v.x, -v.y, -v.z);
    }
#pragma endregion

#pragma region quaternion
    struct quaternion
    {
        _float x, y, z, w;

        quaternion()
            : x(0.f), y(0.f), z(0.f), w(1.f)
        {
        }

        quaternion(_float _x, _float _y, _float _z, _float _w)
            : x(_x), y(_y), z(_z), w(_w)
        {
        }

        quaternion(const XMFLOAT4& q)
            : x(q.x), y(q.y), z(q.z), w(q.w)
        {
        }

        quaternion(const _vector vec)
        {
            _float4 temp;
            XMStoreFloat4(&temp, vec);
            x = temp.x; y = temp.y; z = temp.z; w = temp.w;
        }

        const _float4 toFloat4()
        {
            return _float4(x, y, z, w);
        }

        _vector toXMVector()
        {
            _vector result = XMVectorSet(x, y, z, w);

            return result;
        }

        quaternion& operator=(const quaternion& rhs)
        {
            x = rhs.x; y = rhs.y; z = rhs.z; w = rhs.w;
            return *this;
        }

        quaternion& operator=(const XMFLOAT4& rhs)
        {
            x = rhs.x; y = rhs.y; z = rhs.z; w = rhs.w;
            return *this;
        }

        bool operator==(const quaternion& rhs) const
        {
            return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
        }

        bool operator!=(const quaternion& rhs) const
        {
            return !(*this == rhs);
        }

        quaternion operator*(const quaternion& rhs) const
        {
            XMVECTOR q1 = XMVectorSet(x, y, z, w);
            XMVECTOR q2 = XMVectorSet(rhs.x, rhs.y, rhs.z, rhs.w);

            XMVECTOR qResult = XMQuaternionMultiply(q1, q2);

            return quaternion(qResult);
        }

        quaternion& operator-=(const quaternion& rhs)
        {
            XMVECTOR q1 = XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(this));
            XMVECTOR q2 = XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&rhs));

            XMVECTOR qResult = XMVectorSubtract(q1, q2);

            XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(this), qResult);
            return *this;
        }

        quaternion& operator*=(const quaternion& rhs)
        {
            *this = *this * rhs;
            return *this;
        }

        quaternion normalized() const
        {
            XMVECTOR q = XMVectorSet(x, y, z, w);

            XMVECTOR qNormalized = XMQuaternionNormalize(q);

            return quaternion(qNormalized);
        }

        quaternion conjugated() const
        {
            return quaternion(-x, -y, -z, w);
        }

        float dot(const quaternion& rhs) const
        {
            XMVECTOR q1 = XMVectorSet(x, y, z, w);
            XMVECTOR q2 = XMVectorSet(rhs.x, rhs.y, rhs.z, rhs.w);

            XMVECTOR result = XMQuaternionDot(q1, q2);

            return XMVectorGetX(result);
        }

        static quaternion identity()
        {
            return quaternion(0.f, 0.f, 0.f, 1.f);
        }

        static quaternion from_euler(const vector3& euler_deg)
        {
            return from_euler(euler_deg.x, euler_deg.y, euler_deg.z);
        }

        static quaternion from_euler(float _pitch, float _yaw, float _roll)
        {
            const _vector qx = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), XMConvertToRadians(_pitch));
            const _vector qy = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XMConvertToRadians(_yaw));
            const _vector qz = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0), XMConvertToRadians(_roll));

            const _vector q = XMQuaternionMultiply
            (
                XMQuaternionMultiply(qy, qx),
                qz
            );

            quaternion out;
            XMStoreFloat4(reinterpret_cast<_float4*>(&out), XMQuaternionNormalize(q));
            return out;
        }

        static quaternion from_axis_angle(const vector3& _axis, const _float _radians)
        {
            _vector q = XMQuaternionRotationAxis(_axis.toXMVector(), _radians);

            quaternion result;
            XMStoreFloat4(reinterpret_cast<_float4*>(&result), q);
            return result;
        }

        static quaternion Slerp(const quaternion& _q1, const quaternion& _q2, const _float _t)
        {
            float dot = _q1.x * _q2.x + _q1.y * _q2.y + _q1.z * _q2.z + _q1.w * _q2.w;

            // 만약 dot < 0이면 반대방향 쿼터니언을 사용 (단일 해 선택)
            quaternion q2b = _q2;
            if (dot < 0.0f)
            {
                dot = -dot;
                q2b.x = -q2b.x;
                q2b.y = -q2b.y;
                q2b.z = -q2b.z;
                q2b.w = -q2b.w;
            }

            const float DOT_THRESHOLD = 0.9995f;
            if (dot > DOT_THRESHOLD)
            {
                quaternion result;
                result.x = _q1.x + _t * (q2b.x - _q1.x);
                result.y = _q1.y + _t * (q2b.y - _q1.y);
                result.z = _q1.z + _t * (q2b.z - _q1.z);
                result.w = _q1.w + _t * (q2b.w - _q1.w);

                // Normalize
                _float mag = sqrtf(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
                result.x /= mag;
                result.y /= mag;
                result.z /= mag;
                result.w /= mag;

                return result;
            }

            _float theta_0 = acosf(dot);
            _float theta = theta_0 * _t;

            _float sin_theta = sinf(theta);
            _float sin_theta_0 = sinf(theta_0);

            _float s0 = cosf(theta) - dot * sin_theta / sin_theta_0;
            _float s1 = sin_theta / sin_theta_0;

            quaternion result;
            result.x = (s0 * _q1.x) + (s1 * q2b.x);
            result.y = (s0 * _q1.y) + (s1 * q2b.y);
            result.z = (s0 * _q1.z) + (s1 * q2b.z);
            result.w = (s0 * _q1.w) + (s1 * q2b.w);

            return result;
        }

        vector3 to_euler() const
        {
            vector3 euler;

            _float4 quat;
            XMStoreFloat4(&quat, *this);

            float sinp = 2.f * (quat.w * quat.x - quat.z * quat.y);

            euler.y = atan2f
            (
                2.f * (quat.w * quat.y + quat.x * quat.z),
                1.f - 2.f * (quat.y * quat.y + quat.z * quat.z)
            );

            if (fabs(sinp) >= 1.f)
                euler.x = XM_PIDIV2 * (sinp > 0 ? 1.f : -1.f);
            else
                euler.x = asinf(sinp);

            euler.z = atan2f
            (
                2.f * (quat.w * quat.z + quat.x * quat.y),
                1.f - 2.f * (quat.x * quat.x + quat.z * quat.z)
            );

            return vector3
            (
                XMConvertToDegrees(euler.x),
                XMConvertToDegrees(euler.y),
                XMConvertToDegrees(euler.z)
            );
        }

        const _float4 to_float4()
        {
            return _float4(x, y, z, w);
        }

        _matrix to_matrix() const
        {
            _vector q = XMVectorSet(x, y, z, w);

            return XMMatrixRotationQuaternion(q);
        }

        operator _float4() const
        {
            return _float4(x, y, z, w);
        }

        operator _vector() const
        {
            return XMVectorSet(x, y, z, w);
        }
    };

#pragma endregion

#pragma region ColorValue
    struct ColorValue
    {
        BYTE r;
        BYTE g;
        BYTE b;
        BYTE a;

        ColorValue()
            : r(255), g(255), b(255), a(255)
        {
        }

        ColorValue(BYTE _r, BYTE _g, BYTE _b, BYTE _a = 255)
            : r(_r), g(_g), b(_b), a(_a)
        {
        }

        constexpr UINT32 ARGB(BYTE a, BYTE r, BYTE g, BYTE b)
        {
            return (static_cast<UINT32>(a) << 24) |
                (static_cast<UINT32>(r) << 16) |
                (static_cast<UINT32>(g) << 8) |
                static_cast<UINT32>(b);
        }

        constexpr _float3 f3Color() noexcept
        {
            return _float3(r / 255.f, g / 255.f, b / 255.f);
        }

        constexpr _float4 f4Color() noexcept
        {
            return _float4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
        }

        const _vector toXMVector() const
        {
            return XMVectorSet(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
        }

        //COLORREF rColor() const
        //{
        //    return RGB(r, g, b); 
        //}

        D3DCOLORVALUE dvColor() const
        {
            D3DCOLORVALUE color;
            color.r = float(r / 255.f);
            color.g = float(g / 255.f);
            color.b = float(b / 255.f);
            color.a = float(a / 255.f);
            return color;
        }

        ColorValue(D3DCOLOR d3dColor)
        {
            a = (d3dColor >> 24) & 0xFF;
            r = (d3dColor >> 16) & 0xFF;
            g = (d3dColor >> 8) & 0xFF;
            b = (d3dColor >> 0) & 0xFF;
        }

        ColorValue& operator=(const D3DCOLOR& d3dColor)
        {
            a = (d3dColor >> 24) & 0xFF;
            r = (d3dColor >> 16) & 0xFF;
            g = (d3dColor >> 8) & 0xFF;
            b = (d3dColor >> 0) & 0xFF;
            return *this;
        }

        //operator D3DCOLOR() const
        //{
        //    return D3DCOLOR_ARGB(a, r, g, b);
        //}

        //D3DCOLOR value() const
        //{
        //    return D3DCOLOR_ARGB(a, r, g, b);
        //}

        static ColorValue red() { return ColorValue(255, 0, 0, 255); }
        static ColorValue green() { return ColorValue(0, 255, 0, 255); }
        static ColorValue blue() { return ColorValue(0, 0, 255, 255); }
        static ColorValue white() { return ColorValue(255, 255, 255, 255); }
        static ColorValue black() { return ColorValue(0, 0, 0, 255); }
        static ColorValue yellow() { return ColorValue(255, 255, 0, 255); }
        static ColorValue cyan() { return ColorValue(0, 255, 255, 255); }
        static ColorValue gray() { return ColorValue(128, 128, 128, 255); }
        static ColorValue gray(float _value)
        {
            BYTE v = BYTE(255 * _value);
            return ColorValue(v, v, v, 255);
        }

        static ColorValue magenta() { return ColorValue(255, 0, 255, 255); }
        static ColorValue transparent() { return ColorValue(0, 0, 0, 0); }
    };
#pragma endregion

#pragma region VertexBuffer
    struct VertexTexColorBuffer
    {
        _float3 position;
        _float2 uv;

        static const _uint numElements = 2;
        static constexpr D3D11_INPUT_ELEMENT_DESC elementDesc[numElements] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
    };

    struct VertexTexNormalBuffer
    {
        _float3 position;
        _float3 normal;
        _float2 uv;

        static const _uint numElements = 3;
        static constexpr D3D11_INPUT_ELEMENT_DESC elementDesc[numElements] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
    };

    struct VertexTexNormalTangentBuffer
    {
        _float3 position;
        _float3 normal;
        _float2 uv;
        _float3 tangent;

        static const _uint numElements = 4;
        static constexpr D3D11_INPUT_ELEMENT_DESC elementDesc[numElements] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
    };

    struct VertexColorSkinnedBuffer
    {
        _float3 position;
        _float3 normal;
        _float3 tangent;
        UINT boneIndices[4] = { 0,0,0,0 };
        _float boneWeights[4] = { 0,0,0,0 };

        static const _uint numElements = 5;
        static constexpr D3D11_INPUT_ELEMENT_DESC elementDesc[numElements] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
    };

    struct VertexSkinnedBuffer
    {
        _float3 position;
        _float3 normal;
        _float2 uv;
        _float3 tangent;
        _uint boneIndices[4] = { 0,0,0,0 };
        _float boneWeights[4] = { 0,0,0,0 };

        static const _uint numElements = 6;
        static constexpr D3D11_INPUT_ELEMENT_DESC elementDesc[numElements] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
    };

    struct VertexSkinnedOutlineBuffer
    {
        _float3 position;
        _float3 normal;
        _float2 uv;
        _float3 tangent;
        UINT boneIndices[4] = { 0,0,0,0 };
        _float boneWeights[4] = { 0,0,0,0 };
        _float lineWidth = 1.f;

        static const _uint numElements = 7;
        static constexpr D3D11_INPUT_ELEMENT_DESC elementDesc[numElements] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 76, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
    };
#pragma endregion;

#pragma region ShaderBuffer
    struct MatrixCB
    {
        _matrix world;
    };

    struct CameraCB
    {
        _float3 camPos;
        _matrix view;
        _matrix proj;
        _float padding;
    };

    struct MaterialCB
    {
        _float4 baseColor;
        _uint  useTexture;
        _uint  boneCount;
        _float2 padding;
    };

    struct LightCB
    {
        _matrix lights[64];
    };

    struct ShadowCB
    {
        _float4x4 shadowViewProj;
        _float2 invShadowMapSize;
        _float bias;
        _float padding;
    };

    struct ImageCB
    {
        _float4 values;
    };
#pragma endregion

    inline _float Lerp(_float a, _float b, _float t)
    {
        return a + (b - a) * t;
    }

#pragma region FBXResources
    enum MeshBundleHex { FILTER_MESHBUFFER = 0x000001, FILTER_MATERIAL = 0x000010, FILTER_TEXTURE = 0x000100, FILTER_BONE = 0x001000 };

    struct MeshBundle
    {
        class CMeshBuffer* meshBuffer = nullptr;
        CMaterial* material = nullptr;
        CTexture* texture = nullptr;
    };

    struct SkinnedMeshBundle
    {
        class CSkinnedMeshBuffer* meshBuffer = nullptr;
        CMaterial* material = nullptr;
        CTexture* texture = nullptr;
    };
#pragma endregion;

    typedef struct tagIndex16
    {
        _ushort  _0;
        _ushort  _1;
        _ushort  _2;

    }INDEX16;

    typedef struct tagIndex32
    {
        _ulong	_0;
        _ulong	_1;
        _ulong	_2;
    }INDEX32;
}
#endif // Engine_Struct_h__
