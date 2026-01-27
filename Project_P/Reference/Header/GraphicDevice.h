#pragma once

#include "Object.h"

NS_BEGIN(Engine)

class ENGINE_DLL CGraphicDevice final : public UObject
{
public:
	struct SwapChainSet
	{
		HWND hwnd = nullptr;
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
		D3D11_VIEWPORT viewport;
	};

	SINGLETONCLASS(CGraphicDevice);

public:
	HRESULT Initialize();
	void Destroy();

public:
	HRESULT Ready_GraphicDevice(HWND _hWnd, vector2Int _resolution);

	void Set_RenderTarget(HWND _hWnd);
	HRESULT Clear_BackBuffer_View(const ColorValue* _clearColor);
	HRESULT Clear_DepthStencil_View();
	HRESULT Present();

public:
	ID3D11Device* Get_Device() const;
	ID3D11DeviceContext* Get_Context() const;
	SpriteBatch* Get_SpriteBatch() const;

	HRESULT Add_SwapChain(HWND _hWnd, WINMODE _isWindowed, _uint _winWidth, _uint _winHeight, vector2Int _offsetMin = vector2Int::zero(), vector2Int _offsetMax = vector2Int::zero());

	const D3D11_VIEWPORT* Get_CurrentViewport();
	const D3D11_VIEWPORT* Get_GameViewport();
	const D3D11_VIEWPORT* Get_EditorViewport();

	ID3D11DepthStencilState* Get_DepthStencil_NoWrite() const;
	ID3D11RasterizerState* Get_Rasterizer_CullFront() const;

	ID3D11RenderTargetView* Get_BackBuffer_RTV() const;
	ID3D11DepthStencilView* Get_DepthStencil_DSV() const;

private:
	HRESULT Ready_BackBufferRenderTargetView();
	HRESULT Ready_DepthStencilView(_uint _winWidth, _uint _winHeight);

private:
	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pContext;
	SpriteBatch* m_pSpriteBatch;

	IDXGISwapChain* m_pSwapChain = { nullptr };

	ID3D11RenderTargetView* m_pBackBufferRTV;
	ID3D11DepthStencilView* m_pDepthStencilView;

	unordered_map<HWND, SwapChainSet> m_mSwapChains;

	HWND m_hCrtWndow;

	ID3D11DepthStencilState* m_pDepthStencilNoWrite;
	ID3D11RasterizerState* m_pRasterizerCullFront;
};

NS_END
