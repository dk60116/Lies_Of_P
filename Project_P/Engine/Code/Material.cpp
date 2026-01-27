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
	, m_vTextureList({})
	, m_vBaseColor(ColorValue::white().f4Color())
	, m_mFloatValues(_other.m_mFloatValues)
	, m_mIntValues(_other.m_mIntValues)
	, m_mVector2Values(_other.m_mVector2Values)
	, m_mVector3Values(_other.m_mVector3Values)
	, m_mVector4Values(_other.m_mVector4Values)
	, m_mMatrixValues(_other.m_mMatrixValues)
{
	m_strName = L"Material (Clone)";

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

	mat.baseColor = m_vBaseColor;
	mat.useTexture = (!m_vTextureList.empty() && m_vTextureList[0] != nullptr);
	mat.boneCount = _boneCount;

	context->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &mat, 0, 0);
	context->VSSetConstantBuffers(2, 1, &m_pMaterialBuffer);
	context->PSSetConstantBuffers(2, 1, &m_pMaterialBuffer);

	if (m_vCustomBufferByteList.size() > 0)
		Bind_CustomValues();
}

void CMaterial::Bind_Light(_matrix* _lights, const _uint _count)
{
	if (!_lights || !m_bUseLight || !m_pLightBuffer)
		return;

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

	LightCB buffer = {};

	// 데이터 복사
	const _uint maxCount = min(_count, 64u);
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
	// TODO: Vector4 등도 추가 가능

	// 정렬 맞추기 (16바이트 단위)
	while (m_vCustomBufferByteList.size() % 16 != 0)
		m_vCustomBufferByteList.push_back(0);

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	context->UpdateSubresource(m_pCustomBuffer, 0, nullptr, m_vCustomBufferByteList.data(), 0, 0);
	context->PSSetConstantBuffers(10, 1, &m_pCustomBuffer);
}

const _bool CMaterial::IsUseLight() const
{
	return m_bUseLight;
}

CTexture* CMaterial::Get_Texture(_int _index) const
{
	return m_vTextureList[_index];
}

void CMaterial::Set_Texture(CTexture* _texture, _int _index)
{
	if (_index < 0)
		return;

	while (m_vTextureList.size() <= _index)
		m_vTextureList.push_back(nullptr);

	Safe_Release(m_vTextureList[_index]);
	m_vTextureList[_index] = _texture;

	if (_texture)
		_texture->AddRef();
}

void CMaterial::Set_BaseColor(const _float4& _color)
{
	m_vBaseColor = _color;
}

void CMaterial::Set_FloatValue(const wstring _key, const _float _value)
{
	auto it = m_mFloatValues.find(_key);

	if (it != m_mFloatValues.end())
		m_mFloatValues[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_FloatValue Failed - Key not found: " + _key  + L" - " + m_strResourceName);
}

void CMaterial::Set_IntValue(const wstring _key, const _int _value)
{
	auto it = m_mIntValues.find(_key);

	if (it != m_mIntValues.end())
		m_mIntValues[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_IntValue Failed - Key not found: " + _key + L" - " + m_strResourceName);
}

void CMaterial::Set_Vector2Value(const wstring _key, const _float2 _value)
{
	auto it = m_mVector2Values.find(_key);

	if (it != m_mVector2Values.end())
		m_mVector2Values[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_Vector2Value Failed - Key not found: " + _key + L" - " + m_strResourceName);
}

void CMaterial::Set_Vector3Value(const wstring _key, const _float3 _value)
{
	auto it = m_mVector3Values.find(_key);

	if (it != m_mVector3Values.end())
		m_mVector3Values[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_Vector3Value Failed - Key not found: " + _key + L" - " + m_strResourceName);
}

void CMaterial::Set_Vector4Value(const wstring _key, const _float4 _value)
{
	auto it = m_mVector4Values.find(_key);

	if (it != m_mVector4Values.end())
		m_mVector4Values[_key] = _value;
	else
		CDebug::LogError(L"Material - Set_Vector4Value Failed - Key not found: " + _key + L" - " + m_strResourceName);
}

void CMaterial::Set_MatrixValue(const wstring _key, const _float4x4 _value)
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

	ID3D11ShaderResourceView* texture = nullptr;

	if (!m_vTextureList.empty() && m_vTextureList[0])
		texture = m_vTextureList[0]->Get_SRV();

	context->PSSetShaderResources(0, 1, &texture);

	static ID3D11SamplerState* gSamplerState = nullptr;

	if (!gSamplerState)
	{
		D3D11_SAMPLER_DESC sampleDesc = {};
		sampleDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampleDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampleDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampleDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampleDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		sampleDesc.MinLOD = 0;
		sampleDesc.MaxLOD = D3D11_FLOAT32_MAX;

		if (FAILED(CGraphicDevice::GetInstance().Get_Device()->CreateSamplerState(&sampleDesc, &gSamplerState)))
		{
			CDebug::LogError(L"Create failed SamplerState in Material");
			return;
		}
	}

	context->PSSetSamplers(0, 1, &gSamplerState);
}
