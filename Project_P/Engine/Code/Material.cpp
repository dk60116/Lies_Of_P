#include "epch.h"
#include "Material.h"
#include "Shader.h"

CMaterial::CMaterial()
	: m_pShader(nullptr)
	, m_pMatrixBuffer(nullptr)
	, m_pCameraBuffer(nullptr)
	, m_pMaterialBuffer(nullptr)
	, m_pLightBuffer(nullptr)
	, m_pCustomBuffer(nullptr)
	, m_vCustomBufferByteList({})
	, m_bUseLight(false)
	, m_bUseNormalMap(false)
	, m_bUseORMMap(false)
	, m_vBaseColor(ColorValue::white().f4Color())
	, m_vTextureList({})
	, m_mIntValues({})
	, m_mFloatValues({})
	, m_mVector2Values({})
	, m_mVector3Values({})
	, m_mVector4Values({})
	, m_mMatrixValues({})
{
	m_strName = L"Material";
}

CMaterial::CMaterial(const CMaterial& _other)
	: m_pShader(_other.m_pShader)
	, m_pMatrixBuffer(nullptr)
	, m_pCameraBuffer(nullptr)
	, m_pMaterialBuffer(nullptr)
	, m_pLightBuffer(nullptr)
	, m_pCustomBuffer(nullptr)
	, m_vCustomBufferByteList(_other.m_vCustomBufferByteList)
	, m_bUseLight(_other.m_bUseLight)
	, m_bUseNormalMap(_other.m_bUseNormalMap)
	, m_bUseORMMap(_other.m_bUseORMMap)
	, m_vTextureList({})
	, m_vBaseColor(_other.m_vBaseColor)
	, m_mFloatValues(_other.m_mFloatValues)
	, m_mIntValues(_other.m_mIntValues)
	, m_mVector2Values(_other.m_mVector2Values)
	, m_mVector3Values(_other.m_mVector3Values)
	, m_mVector4Values(_other.m_mVector4Values)
	, m_mMatrixValues(_other.m_mMatrixValues)
{
	m_strName = L"Material (Clone)";
	m_strResourceName = _other.m_strResourceName;
	m_strFilePath = _other.m_strFilePath;

	if (m_pShader)
		m_pShader->AddRef();

	Create_ConstantBuffer();
}

CMaterial::~CMaterial()
{
	OnDestroy();
}

CMaterial* CMaterial::Create(const wstring _path)
{
	return new CMaterial();
}

CMaterial* CMaterial::Clone(const CMaterial& _other)
{
	return new CMaterial(_other);
}

HRESULT CMaterial::Initialize(const wstring& _name, wstring _filePath, void* _desc)
{
	if (FAILED(__super::Initialize(_name, _filePath, _desc)))
		return E_FAIL;

	if (_desc)
	{
		MATERIALDESC* matDesc = reinterpret_cast<MATERIALDESC*>(_desc);

		if (!matDesc->shaderPointer)
			return E_FAIL;

		Set_Shader(matDesc->shaderPointer);
		m_bUseLight = matDesc->usingRight;
		m_bUseNormalMap = matDesc->usingNormalMap;
		m_bUseORMMap = matDesc->usingORMMap;

		for (const auto& [key, value] : matDesc->customFloatValues)
		{
			m_mFloatValues.emplace(key, value);

			const BYTE* p = reinterpret_cast<const BYTE*>(&value);

			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), p, p + sizeof(float));
		}
		for (const auto& [key, value] : matDesc->customIntValues)
		{
			m_mIntValues.emplace(key, value);

			const BYTE* p = reinterpret_cast<const BYTE*>(&value);

			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), p, p + sizeof(int));
		}
		for (const auto& [key, value] : matDesc->customVector2Values)
		{
			m_mVector2Values.emplace(key, value);

			const BYTE* pX = reinterpret_cast<const BYTE*>(&value.x);
			const BYTE* pY = reinterpret_cast<const BYTE*>(&value.y);

			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pX, pX + sizeof(_float));
			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pY, pY + sizeof(_float));
		}
		for (const auto& [key, value] : matDesc->customVector3Values)
		{
			m_mVector3Values.emplace(key, value);

			const BYTE* pX = reinterpret_cast<const BYTE*>(&value.x);
			const BYTE* pY = reinterpret_cast<const BYTE*>(&value.y);
			const BYTE* pZ = reinterpret_cast<const BYTE*>(&value.z);

			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pX, pX + sizeof(_float));
			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pY, pY + sizeof(_float));
			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pZ, pZ + sizeof(_float));
		}
		for (const auto& [key, value] : matDesc->customVector4Values)
		{
			m_mVector4Values.emplace(key, value);

			const BYTE* pX = reinterpret_cast<const BYTE*>(&value.x);
			const BYTE* pY = reinterpret_cast<const BYTE*>(&value.y);
			const BYTE* pZ = reinterpret_cast<const BYTE*>(&value.z);
			const BYTE* pW = reinterpret_cast<const BYTE*>(&value.w);

			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pX, pX + sizeof(_float));
			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pY, pY + sizeof(_float));
			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pZ, pZ + sizeof(_float));
			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pW, pW + sizeof(_float));
		}
		for (const auto& [key, value] : matDesc->customMatrixValues)
		{
			m_mMatrixValues.emplace(key, value);

			const BYTE* pMat = reinterpret_cast<const BYTE*>(&value);
			m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pMat, pMat + sizeof(_float4x4));
		}
	}

	if (FAILED(Create_ConstantBuffer()))
	{
		CDebug::LogError(L"Material - Create_ConstantBuffer Failed: " + m_strResourceName);
		return E_FAIL;
	}

	return S_OK;
}

void CMaterial::OnDestroy()
{
	Safe_Release(m_pShader);
	Safe_Release(m_pMatrixBuffer);
	Safe_Release(m_pCameraBuffer);
	Safe_Release(m_pMaterialBuffer);
	Safe_Release(m_pLightBuffer);

	m_vCustomBufferByteList.clear();

	for (TRAVERSAL_ITER(m_vTextureList, it))
		Safe_Release(*it);
	m_vTextureList.clear();
}

void CMaterial::Bind_Matrix(const _fmatrix _world)
{
	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

	// b0: PerObject
	MatrixCB matrixCB = {};
	matrixCB.world = XMMatrixTranspose(_world);
	context->UpdateSubresource(m_pMatrixBuffer, 0, nullptr, &matrixCB, 0, 0);
	context->VSSetConstantBuffers(0, 1, &m_pMatrixBuffer);
}

void CMaterial::Bind_Camera(const _float3 _camPos, const _fmatrix _view, const _cmatrix _projection, const _uint _boneCount)
{
	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

	if (m_pShader)
		m_pShader->Bind();

	Bind_Texture();

	// b1: PerCamera
	CameraCB camCB = {};
	camCB.camPos = _camPos;
	camCB.view = XMMatrixTranspose(_view);
	camCB.proj = XMMatrixTranspose(_projection);
	context->UpdateSubresource(m_pCameraBuffer, 0, nullptr, &camCB, 0, 0);
	context->VSSetConstantBuffers(1, 1, &m_pCameraBuffer);
	context->PSSetConstantBuffers(1, 1, &m_pCameraBuffer);

	// b2: PerMaterial
	MaterialCB mat = {};

	if (m_vTextureList.size() >= 3)
	{
		CMaterial* m = this;
		int a = 0;
	}

	mat.baseColor = m_vBaseColor;
	mat.useTexture = (!m_vTextureList.empty() && m_vTextureList[0] != nullptr);
	mat.useNormalMap = (m_bUseNormalMap && m_vTextureList.size() >= 2 && m_vTextureList[1] != nullptr);
	mat.useORMMap = (m_bUseORMMap && m_vTextureList.size() >= 3 && m_vTextureList[2] != nullptr);
	mat.boneCount = _boneCount;

	context->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &mat, 0, 0);
	context->VSSetConstantBuffers(2, 1, &m_pMaterialBuffer);
	context->PSSetConstantBuffers(2, 1, &m_pMaterialBuffer);

	if (m_vCustomBufferByteList.size() > 0)
		Bind_CustomValues();
}

void CMaterial::Bind_Light(_matrix* _lights, const _uint _count)
{
	if (!m_bUseLight || !m_pLightBuffer)
		return;

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

	LightCB buffer = {};
	const _uint maxCount = min(_count, 64u);
	if (_lights && maxCount > 0)
		memcpy(buffer.lights, _lights, sizeof(_matrix) * maxCount);

	context->UpdateSubresource(m_pLightBuffer, 0, nullptr, &buffer, 0, 0);
	context->PSSetConstantBuffers(4, 1, &m_pLightBuffer);
}

void CMaterial::Bind_CustomValues()
{
	m_vCustomBufferByteList.clear();

	// 순서 중요: HLSL과 일치해야 함
	for (const auto& [key, value] : m_mFloatValues)
	{
		const BYTE* p = reinterpret_cast<const BYTE*>(&value);
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), p, p + sizeof(float));
	}
	for (const auto& [key, value] : m_mIntValues)
	{
		const BYTE* p = reinterpret_cast<const BYTE*>(&value);
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), p, p + sizeof(int));
	}
	for (const auto& [key, value] : m_mVector2Values)
	{
		const BYTE* px = reinterpret_cast<const BYTE*>(&value.x);
		const BYTE* py = reinterpret_cast<const BYTE*>(&value.y);
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), px, px + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), py, py + sizeof(float));
	}
	for (const auto& [key, value] : m_mVector3Values)
	{
		const BYTE* px = reinterpret_cast<const BYTE*>(&value.x);
		const BYTE* py = reinterpret_cast<const BYTE*>(&value.y);
		const BYTE* pz = reinterpret_cast<const BYTE*>(&value.z);
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), px, px + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), py, py + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pz, pz + sizeof(float));
	}
	for (const auto& [key, value] : m_mVector4Values)
	{
		const BYTE* px = reinterpret_cast<const BYTE*>(&value.x);
		const BYTE* py = reinterpret_cast<const BYTE*>(&value.y);
		const BYTE* pz = reinterpret_cast<const BYTE*>(&value.z);
		const BYTE* pw = reinterpret_cast<const BYTE*>(&value.w);
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), px, px + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), py, py + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pz, pz + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), pw, pw + sizeof(float));
	}
	for (const auto& [key, value] : m_mMatrixValues)
	{
		const BYTE* _11 = reinterpret_cast<const BYTE*>(&value._11);
		const BYTE* _12 = reinterpret_cast<const BYTE*>(&value._12);
		const BYTE* _13 = reinterpret_cast<const BYTE*>(&value._13);
		const BYTE* _14 = reinterpret_cast<const BYTE*>(&value._14);

		const BYTE* _21 = reinterpret_cast<const BYTE*>(&value._21);
		const BYTE* _22 = reinterpret_cast<const BYTE*>(&value._22);
		const BYTE* _23 = reinterpret_cast<const BYTE*>(&value._23);
		const BYTE* _24 = reinterpret_cast<const BYTE*>(&value._24);

		const BYTE* _31 = reinterpret_cast<const BYTE*>(&value._31);
		const BYTE* _32 = reinterpret_cast<const BYTE*>(&value._32);
		const BYTE* _33 = reinterpret_cast<const BYTE*>(&value._33);
		const BYTE* _34 = reinterpret_cast<const BYTE*>(&value._34);

		const BYTE* _41 = reinterpret_cast<const BYTE*>(&value._41);
		const BYTE* _42 = reinterpret_cast<const BYTE*>(&value._42);
		const BYTE* _43 = reinterpret_cast<const BYTE*>(&value._43);
		const BYTE* _44 = reinterpret_cast<const BYTE*>(&value._44);

		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _11, _11 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _12, _12 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _13, _13 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _14, _14 + sizeof(float));

		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _21, _21 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _22, _22 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _23, _23 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _24, _24 + sizeof(float));

		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _31, _31 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _32, _32 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _33, _33 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _34, _34 + sizeof(float));

		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _41, _41 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _42, _42 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _43, _43 + sizeof(float));
		m_vCustomBufferByteList.insert(m_vCustomBufferByteList.end(), _44, _44 + sizeof(float));
	}

	while (m_vCustomBufferByteList.size() % 16 != 0)
		m_vCustomBufferByteList.push_back(0);

	if (!m_pCustomBuffer || m_vCustomBufferByteList.empty())
		return;

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	context->UpdateSubresource(m_pCustomBuffer, 0, nullptr, m_vCustomBufferByteList.data(), 0, 0);
	context->PSSetConstantBuffers(10, 1, &m_pCustomBuffer);
}

CShader* CMaterial::Get_Shader() const
{
	return m_pShader;
}

const _uint CMaterial::Get_TextureCount() const
{
	return static_cast<_uint>(m_vTextureList.size());
}

const _bool CMaterial::IsUseLight() const
{
	return m_bUseLight;
}

CTexture* CMaterial::Get_Texture(_int _index) const
{
	if (_index < 0 || static_cast<size_t>(_index) >= m_vTextureList.size())
		return nullptr;

	return m_vTextureList[_index];
}

const _float CMaterial::Get_FloatValue(const wstring& _key) const
{
	auto it = m_mFloatValues.find(_key);

	if (it != m_mFloatValues.end())
		return (*it).second;
	else
		CDebug::LogError(L"Material - Get_FloatValue Failed - Key not found: " + _key + L" - " + m_strResourceName);

	return 0.f;
}

const _int CMaterial::Get_IntValue(const wstring& _key) const
{
	auto it = m_mIntValues.find(_key);

	if (it != m_mIntValues.end())
		return (*it).second;
	else
		CDebug::LogError(L"Material - Get_IntValue Failed - Key not found: " + _key + L" - " + m_strResourceName);

	return 0;
}

const _float2 CMaterial::Get_Vector2Value(const wstring& _key) const
{
	auto it = m_mVector2Values.find(_key);

	if (it != m_mVector2Values.end())
		return (*it).second;
	else
		CDebug::LogError(L"Material - Get_Vector2Value Failed - Key not found: " + _key + L" - " + m_strResourceName);

	return {};
}

const _float3& CMaterial::Get_Vector3Value(const wstring& _key)
{
	auto it = m_mVector3Values.find(_key);

	if (it != m_mVector3Values.end())
		return (*it).second;
	else
		CDebug::LogError(L"Material - Get_Vector3Value Failed - Key not found: " + _key + L" - " + m_strResourceName);

	return {};
}

const _float4& CMaterial::Get_Vector4Value(const wstring& _key)
{
	auto it = m_mVector4Values.find(_key);

	if (it != m_mVector4Values.end())
		return (*it).second;
	else
		CDebug::LogError(L"Material - Get_Vector4Value Failed - Key not found: " + _key + L" - " + m_strResourceName);

	return {};
}

const _float4x4& CMaterial::Get_MatrixValue(const wstring& _key)
{
	auto it = m_mMatrixValues.find(_key);

	if (it != m_mMatrixValues.end())
		return (*it).second;
	else
		CDebug::LogError(L"Material - Get_MatrixValue Failed - Key not found: " + _key + L" - " + m_strResourceName);

	return {};
}

const unordered_map<wstring, _float>& CMaterial::Get_FloatValues() const
{
	return m_mFloatValues;
}

const unordered_map<wstring, _int>& CMaterial::Get_IntValues() const
{
	return m_mIntValues;
}

const unordered_map<wstring, _float2>& CMaterial::Get_Vector2Values() const
{
	return m_mVector2Values;
}

const unordered_map<wstring, _float3>& CMaterial::Get_Vector3Values() const
{
	return m_mVector3Values;
}

const unordered_map<wstring, _float4>& CMaterial::Get_Vector4Values() const
{
	return m_mVector4Values;
}

const unordered_map<wstring, _float4x4>& CMaterial::Get_MatrixValues() const
{
	return m_mMatrixValues;
}

void CMaterial::Set_Texture(CTexture* _texture, _int _index)
{
	if (_index < 0)
		return;

	while (m_vTextureList.size() <= _index)
		m_vTextureList.push_back(nullptr);

	if (m_vTextureList.size() < _index + 1)
		m_vTextureList.resize(_index + 1);

	Safe_Release(m_vTextureList[_index]);
	m_vTextureList[_index] = _texture;

	if (_texture)
		_texture->AddRef();
}

void CMaterial::Remove_Texture(_int _index)
{
	if (_index < 0 || static_cast<size_t>(_index) >= m_vTextureList.size())
		return;

	Safe_Release(m_vTextureList[_index]);
	m_vTextureList.erase(m_vTextureList.begin() + _index);
}

void CMaterial::Set_BaseColor(const _float4& _color)
{
	m_vBaseColor = _color;
}

void CMaterial::Set_FloatValue(const wstring& _key, const _float _value)
{
	auto it = m_mFloatValues.find(_key);

	if (it != m_mFloatValues.end())
		m_mFloatValues[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_FloatValue Failed - Key not found: " + _key  + L" - " + m_strResourceName);
}

void CMaterial::Set_IntValue(const wstring& _key, const _int _value)
{
	auto it = m_mIntValues.find(_key);

	if (it != m_mIntValues.end())
		m_mIntValues[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_IntValue Failed - Key not found: " + _key + L" - " + m_strResourceName);
}

void CMaterial::Set_Vector2Value(const wstring& _key, const _float2 _value)
{
	auto it = m_mVector2Values.find(_key);

	if (it != m_mVector2Values.end())
		m_mVector2Values[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_Vector2Value Failed - Key not found: " + _key + L" - " + m_strResourceName);
}

void CMaterial::Set_Vector3Value(const wstring& _key, const _float3& _value)
{
	auto it = m_mVector3Values.find(_key);

	if (it != m_mVector3Values.end())
		m_mVector3Values[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_Vector3Value Failed - Key not found: " + _key + L" - " + m_strResourceName);
}

void CMaterial::Set_Vector4Value(const wstring& _key, const _float4& _value)
{
	auto it = m_mVector4Values.find(_key);

	if (it != m_mVector4Values.end())
		m_mVector4Values[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_Vector4Value Failed - Key not found: " + _key + L" - " + m_strResourceName);
}

void CMaterial::Set_MatrixValue(const wstring& _key, const _float4x4& _value)
{
	auto it = m_mMatrixValues.find(_key);

	if (it != m_mMatrixValues.end())
		m_mMatrixValues[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_MatrixValue Failed - Key not found: " + _key + L" - " + m_strResourceName);
}

void CMaterial::Set_Shader(CShader* _shader)
{
	if (_shader == m_pShader)
		return;

	Safe_Release(m_pShader);

	if (_shader)
	{
		m_pShader = _shader;
		m_pShader->AddRef();
	}
}

HRESULT CMaterial::Create_ConstantBuffer()
{
	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	// b0 : MatrixCB (VS)
	desc.ByteWidth = sizeof(MatrixCB);
	if (FAILED(device->CreateBuffer(&desc, nullptr, &m_pMatrixBuffer)))
		return E_FAIL;

	// b1 : View/Proj Matrix (VS)
	desc.ByteWidth = sizeof(CameraCB);
	if (FAILED(device->CreateBuffer(&desc, nullptr, &m_pCameraBuffer)))
		return E_FAIL;

	// b2 : MaterialCB (PS)
	desc.ByteWidth = sizeof(MaterialCB);
	if (FAILED(device->CreateBuffer(&desc, nullptr, &m_pMaterialBuffer)))
		return E_FAIL;

	// b4: Light (PS)
	if (m_bUseLight)
	{
		desc.ByteWidth = sizeof(LightCB);

		if (FAILED(device->CreateBuffer(&desc, nullptr, &m_pLightBuffer)))
			return E_FAIL;
	}

	// b10: Custom
	if (m_vCustomBufferByteList.size() > 0)
	{
		_uint byteWidth = static_cast<_uint>(m_vCustomBufferByteList.size());
		byteWidth = (byteWidth + 15) & ~15;
		
		desc.ByteWidth = byteWidth;

		if (FAILED(device->CreateBuffer(&desc, nullptr, &m_pCustomBuffer)))
			return E_FAIL;

		if (!m_pCustomBuffer)
		{
			CDebug::LogError(L"Material - Create_ConstantBuffer Failed - m_pCustomBuffer is null: " + m_strResourceName);
			return E_FAIL;
		}
	}

	return S_OK;
}

void CMaterial::Bind_Texture() const
{
	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

	ID3D11ShaderResourceView* nullSRV[16] = {};
	context->PSSetShaderResources(0, 16, nullSRV);

	for (size_t i = 0; i < m_vTextureList.size(); ++i)
	{
		ID3D11ShaderResourceView* srv = nullptr;
		if (m_vTextureList[i])
		{
			srv = m_vTextureList[i]->Get_SRV();
			context->PSSetShaderResources((_uint)i, 1, &srv);
		}
	}

	static ID3D11SamplerState* gSamplerState = nullptr;
	if (!gSamplerState)
	{
		D3D11_SAMPLER_DESC s = {};
		s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		s.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		s.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		s.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		s.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		s.MinLOD = 0;
		s.MaxLOD = D3D11_FLOAT32_MAX;

		if (FAILED(CGraphicDevice::GetInstance().Get_Device()->CreateSamplerState(&s, &gSamplerState)))
		{
			CDebug::LogError(L"Create failed SamplerState in Material");
			return;
		}
	}

	context->PSSetSamplers(0, 1, &gSamplerState);
}
