#pragma once

#include "EngineResource.h"

NS_BEGIN(Engine)

class ENGINE_DLL CMaterial final : public CEngineResource
{
	friend class CResources;

public:
	typedef struct Material
	{
		class CShader* shaderPointer;
		_bool usingRight = true;
		_bool usingNormalMap = false;
		_bool usingORMMap = false;
		vector<pair<wstring, _float>> customFloatValues = {};
		vector<pair<wstring, _int>> customIntValues = {};
		vector<pair<wstring, _float2>> customVector2Values = {};
		vector<pair<wstring, _float3>> customVector3Values = {};
		vector<pair<wstring, _float4>> customVector4Values = {};
		vector<pair<wstring, _float4x4>> customMatrixValues = {};
	} MATERIALDESC;

private:
	explicit CMaterial();
	CMaterial(const CMaterial& _other);
	virtual ~CMaterial();

private:
	static CMaterial* Create(const wstring _path = L"");
	static CMaterial* Clone(const CMaterial& _other);

private:
	HRESULT Initialize(const wstring& _name, wstring _filePath, void* _desc);
	void OnDestroy() override;

public:
	void Bind_Matrix(const _fmatrix _world);
	void Bind_Camera(const _float3 _camPos, const _fmatrix _view, const _cmatrix _projection, const _uint _boneCount = 0);
	void Bind_Light(_matrix* _lights, const _uint _count);
	void Bind_CustomValues();

public:
	const _bool IsUseLight() const;
	class CShader* Get_Shader() const;
	const _uint Get_TextureCount() const;
	class CTexture* Get_Texture(_int _index) const;

public:
	const _float Get_FloatValue(const wstring& _key) const;
	const _int Get_IntValue(const wstring& _key) const;
	const _float2 Get_Vector2Value(const wstring& _key) const;
	const _float3& Get_Vector3Value(const wstring& _key);
	const _float4& Get_Vector4Value(const wstring& _key);
	const _float4x4& Get_MatrixValue(const wstring& _key);

	void Set_Shader(CShader* _shader);
	void Set_Texture(CTexture* _texture, _int _index = 0);
	void Remove_Texture(_int _index);
	void Set_BaseColor(const _float4& _color);

	void Set_FloatValue(const wstring& _key, const _float _value);
	void Set_IntValue(const wstring& _key, const _int _value);
	void Set_Vector2Value(const wstring& _key, const _float2 _value);
	void Set_Vector3Value(const wstring& _key, const _float3& _value);
	void Set_Vector4Value(const wstring& _key, const _float4& _value);
	void Set_MatrixValue(const wstring& _key, const _float4x4& _value);

private:
	HRESULT Create_ConstantBuffer();

	void Bind_Texture() const;

private:
	CShader* m_pShader;

	ID3D11Buffer* m_pMatrixBuffer;
	ID3D11Buffer* m_pCameraBuffer;
	ID3D11Buffer* m_pMaterialBuffer;
	ID3D11Buffer* m_pLightBuffer;
	ID3D11Buffer* m_pCustomBuffer;
	vector<BYTE> m_vCustomBufferByteList;

	_bool m_bUseLight, m_bUseNormalMap, m_bUseORMMap;
	_float4 m_vBaseColor;
	vector<class CTexture*> m_vTextureList;
	unordered_map<wstring, _float> m_mFloatValues;
	unordered_map<wstring, _int> m_mIntValues;
	unordered_map<wstring, _float2> m_mVector2Values;
	unordered_map<wstring, _float3> m_mVector3Values;
	unordered_map<wstring, _float4> m_mVector4Values;
	unordered_map<wstring, _float4x4> m_mMatrixValues;
};

NS_END

