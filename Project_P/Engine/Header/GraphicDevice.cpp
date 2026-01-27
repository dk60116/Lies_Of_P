#include "epch.h"
#include "GraphicDevice.h"

CGraphicDevice::CGraphicDevice()
	: m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_pSwapChain(nullptr)
	, m_pBackBufferRTV(nullptr)
	, m_pDepthStencilView(nullptr)
	, m_hCrtWndow(nullptr)
	, m_pDepthStencilNoWrite(nullptr)
	, m_pRasterizerCullFront(nullptr)
{
}

CGraphicDevice::~CGraphicDevice()
{
	Destroy();
}

CGraphicDevice& CGraphicDevice::GetInstance()
{
	static CGraphicDevice inst;
	return inst;
}

HRESULT CGraphicDevice::Initialize()
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	if (FAILED(Ready_GraphicDevice(CDisplay::GetInstance().Get_GameWindow(), CDisplay::GetInstance().Get_ScreenResolution())))
		return E_FAIL;

	vector2Int res = CDisplay::GetInstance().Get_ScreenResolution();
	if (FAILED(Ready_BackBufferRenderTargetView()))  
		return E_FAIL;
	if (FAILED(Ready_DepthStencilView(res.x, res.y))) 
		return E_FAIL;

	m_pContext->OMSetRenderTargets(1, &m_pBackBufferRTV, m_pDepthStencilView);

	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = static_cast<float>(res.x);
	vp.Height = static_cast<float>(res.y);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	m_pContext->RSSetViewports(1, &vp);

	{
		D3D11_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.DepthFunc = D3D11_COMPARISON_LESS;

		HRESULT hr = m_pDevice->CreateDepthStencilState(&dsDesc, &m_pDepthStencilNoWrite);

		if (FAILED(hr))
			return E_FAIL;
	}

	{
		D3D11_RASTERIZER_DESC rsDesc = {};
		rsDesc.FillMode = D3D11_FILL_SOLID;
		rsDesc.CullMode = D3D11_CULL_FRONT;
		rsDesc.DepthClipEnable = TRUE;

		HRESULT hr = m_pDevice->CreateRasterizerState(&rsDesc, &m_pRasterizerCullFront);
		if (FAILED(hr))
			return E_FAIL;
	}

	m_pSpriteBatch = new SpriteBatch(m_pContext);

	return S_OK;
}

void CGraphicDevice::Destroy()
{
	Safe_Release(m_pSwapChain);
	Safe_Release(m_pDepthStencilView);
	Safe_Release(m_pBackBufferRTV);
	Safe_Release(m_pContext);
	Safe_Release(m_pDevice);
	Safe_Release(m_pDepthStencilNoWrite);
	Safe_Release(m_pRasterizerCullFront);

	if (m_pSpriteBatch)
	{
		delete m_pSpriteBatch;
		m_pSpriteBatch = nullptr;
	}

	CoUninitialize();
}

HRESULT CGraphicDevice::Ready_GraphicDevice(HWND _hWnd, vector2Int _resolution)
{
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferDesc.Width = _resolution.x;
	swapDesc.BufferDesc.Height = _resolution.y;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = _hWnd;
	swapDesc.SampleDesc.Count = 1;
	swapDesc.Windowed = TRUE;
	swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	IDXGISwapChain* pSwapChain = nullptr;

	HRESULT hr = D3D11CreateDeviceAndSwapChain
	(
		nullptr, D3D_DRIVER_TYPE_HARDWARE,
		nullptr, 0, nullptr, 0,
		D3D11_SDK_VERSION,
		&swapDesc, &m_pSwapChain,
		&m_pDevice, nullptr, &m_pContext
	);

	if (FAILED(hr))
		return E_FAIL;

	if (pSwapChain)
		pSwapChain->Release();

	return S_OK;
}

void CGraphicDevice::Set_RenderTarget(HWND _hWnd)
{
	auto it = m_mSwapChains.find(_hWnd);
	
	if (it == m_mSwapChains.end())
		return;

	const SwapChainSet& sc = it->second;

	m_pContext->OMSetRenderTargets(1, sc.rtv.GetAddressOf(), sc.dsv.Get());
	m_pContext->RSSetViewports(1, &sc.viewport);

	m_hCrtWndow = _hWnd;
}

HRESULT CGraphicDevice::Clear_BackBuffer_View(const ColorValue* _clearColor)
{
	auto it = m_mSwapChains.find(m_hCrtWndow);
	if (it == m_mSwapChains.end())
		return E_FAIL;

	const SwapChainSet& sc = it->second;
	auto dvColor = _clearColor->dvColor();

	m_pContext->ClearRenderTargetView(sc.rtv.Get(), reinterpret_cast<const _float*>(&dvColor));
	return S_OK;
}

HRESULT CGraphicDevice::Clear_DepthStencil_View()
{
	auto it = m_mSwapChains.find(m_hCrtWndow);
	if (it == m_mSwapChains.end())
		return E_FAIL;

	const SwapChainSet& sc = it->second;
	m_pContext->ClearDepthStencilView(sc.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
	return S_OK;
}

HRESULT CGraphicDevice::Present()
{
	auto it = m_mSwapChains.find(m_hCrtWndow);
	if (it == m_mSwapChains.end())
		return E_FAIL;

	return it->second.swapChain->Present(0, 0);
}

ID3D11Device* CGraphicDevice::Get_Device() const
{
	return m_pDevice;
}

ID3D11DeviceContext* CGraphicDevice::Get_Context() const
{
	return m_pContext;
}

SpriteBatch* CGraphicDevice::Get_SpriteBatch() const
{
	return m_pSpriteBatch;
}

HRESULT CGraphicDevice::Add_SwapChain(HWND _hWnd, WINMODE _isWindowed, _uint _winWidth, _uint _winHeight, vector2Int _offsetMin, vector2Int _offsetMax)
{
	SwapChainSet sc{};
	sc.hwnd = _hWnd;

	// DXGI Factory 생성
	ComPtr<IDXGIDevice> dxgiDevice;
	m_pDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice));

	ComPtr<IDXGIAdapter> adapter;
	dxgiDevice->GetParent(IID_PPV_ARGS(&adapter));

	ComPtr<IDXGIFactory> factory;
	adapter->GetParent(IID_PPV_ARGS(&factory));

	// 스왑체인 생성
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Width = _winWidth;
	sd.BufferDesc.Height = _winHeight;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = _hWnd;
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	ComPtr<IDXGISwapChain> swapChain;

	if (FAILED(factory->CreateSwapChain(m_pDevice, &sd, &swapChain)))
		return E_FAIL;

	sc.swapChain = swapChain;

	// RTV 생성
	ComPtr<ID3D11Texture2D> backBuffer;
	swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

	m_pDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &sc.rtv);

	// DSV 생성
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = _winWidth;
	depthDesc.Height = _winHeight;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ComPtr<ID3D11Texture2D> depthTex;
	m_pDevice->CreateTexture2D(&depthDesc, nullptr, &depthTex);
	m_pDevice->CreateDepthStencilView(depthTex.Get(), nullptr, &sc.dsv);

	// 뷰포트 설정
	sc.viewport.TopLeftX = static_cast<FLOAT>(_offsetMin.x);
	sc.viewport.TopLeftY = static_cast<FLOAT>(_offsetMin.y);
	sc.viewport.Width = static_cast<FLOAT>(_winWidth - _offsetMax.x);
	sc.viewport.Height = static_cast<FLOAT>(_winHeight - _offsetMax.y);
	sc.viewport.MinDepth = 0.0f;
	sc.viewport.MaxDepth = 1.0f;

	m_mSwapChains[_hWnd] = move(sc);

	return S_OK;
}

const D3D11_VIEWPORT* CGraphicDevice::Get_CurrentViewport()
{
	auto it = m_mSwapChains.find(m_hCrtWndow);

	if (it == m_mSwapChains.end())
		return nullptr;

	return &it->second.viewport;
}

const D3D11_VIEWPORT* CGraphicDevice::Get_GameViewport()
{
	auto it = m_mSwapChains.find(CDisplay::GetInstance().Get_GameWindow());

	if (it == m_mSwapChains.end())
		return nullptr;

	return &it->second.viewport;
}

const D3D11_VIEWPORT* CGraphicDevice::Get_EditorViewport()
{
	auto it = m_mSwapChains.find(CDisplay::GetInstance().Get_EditorWindow());

	if (it == m_mSwapChains.end())
		return nullptr;

	return &it->second.viewport;
}

ID3D11DepthStencilState* CGraphicDevice::Get_DepthStencil_NoWrite() const
{
	return m_pDepthStencilNoWrite;
}

ID3D11RasterizerState* CGraphicDevice::Get_Rasterizer_CullFront() const
{
	return m_pRasterizerCullFront;
}

ID3D11RenderTargetView* CGraphicDevice::Get_BackBuffer_RTV() const
{
	return m_pBackBufferRTV;
}

ID3D11DepthStencilView* CGraphicDevice::Get_DepthStencil_DSV() const
{
	return m_pDepthStencilView;
}

HRESULT CGraphicDevice::Ready_BackBufferRenderTargetView()
{
	if (nullptr == m_pDevice)
		return E_FAIL;

	ID3D11Texture2D* pBackBufferTexture = nullptr;

	if (FAILED(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture)))
		return E_FAIL;

	if (FAILED(m_pDevice->CreateRenderTargetView(pBackBufferTexture, nullptr, &m_pBackBufferRTV)))
		return E_FAIL;

	Safe_Release(pBackBufferTexture);

	return S_OK;
}

HRESULT CGraphicDevice::Ready_DepthStencilView(_uint _winWidth, _uint _winHeight)
{
	if (nullptr == m_pDevice)
		return E_FAIL;

	ID3D11Texture2D* pDepthStencilTexture = nullptr;

	D3D11_TEXTURE2D_DESC	TextureDesc;
	ZeroMemory(&TextureDesc, sizeof(D3D11_TEXTURE2D_DESC));

	TextureDesc.Width = _winWidth;
	TextureDesc.Height = _winHeight;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.SampleDesc.Count = 1;

	TextureDesc.Usage = D3D11_USAGE_DEFAULT;

	TextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	if (FAILED(m_pDevice->CreateTexture2D(&TextureDesc, nullptr, &pDepthStencilTexture)))
		return E_FAIL;

	if (FAILED(m_pDevice->CreateDepthStencilView(pDepthStencilTexture, nullptr, &m_pDepthStencilView)))
		return E_FAIL;

	Safe_Release(pDepthStencilTexture);

	return S_OK;
}
