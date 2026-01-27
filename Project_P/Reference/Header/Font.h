#pragma once
#include "EngineResource.h"

NS_BEGIN(Engine)

class ENGINE_DLL CFont final : public CEngineResource
{
	friend class CResources;

protected:
	CFont();
	~CFont();

protected:
	static CFont* Create();
	HRESULT Initialize(const wstring& _name, const wstring& _filePath, void* _desc) override;
	void OnDestroy() override;

public:
	void Render();

public:
	SpriteFont* Get_SpriteFont() const;

private:
	SpriteFont* m_pSpriteFont;
};

NS_END

