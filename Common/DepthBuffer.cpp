#include "DepthBuffer.h"
#include "d3dApp.h"

DepthBuffer::DepthBuffer(const std::string & Name)
	: m_name(Name)
{
	g_Device->GetImmediateContext(&m_pContext);
}

void DepthBuffer::Create(uint32_t Width, uint32_t Height, DXGI_FORMAT Format)
{
	D3D11_TEXTURE2D_DESC DepthStencilDesc;
	
	DepthStencilDesc.Width     = Width;
	DepthStencilDesc.Height    = Height;
	DepthStencilDesc.MipLevels = 1;
	DepthStencilDesc.ArraySize = 1;
	DepthStencilDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;

	if (m_Enable4xMsaa)
	{
		DepthStencilDesc.SampleDesc.Count   = 4;
		DepthStencilDesc.SampleDesc.Quality = m_4xMsaaQuality-1;
	}
	// No MSAA
	else
	{
		DepthStencilDesc.SampleDesc.Count   = 1;
		DepthStencilDesc.SampleDesc.Quality = 0;
	}

	DepthStencilDesc.Usage          = D3D11_USAGE_DEFAULT;
	DepthStencilDesc.BindFlags      = D3D11_BIND_DEPTH_STENCIL;
	DepthStencilDesc.CPUAccessFlags = 0; 
	DepthStencilDesc.MiscFlags      = 0;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> Tex = nullptr;
	HR(g_Device->CreateTexture2D(&DepthStencilDesc, 0, Tex.GetAddressOf()));
	HR(g_Device->CreateDepthStencilView(Tex.Get(), 0, m_DSV.GetAddressOf()));
	// UNDONE
	// HR(g_Device->CreateShaderResourceView(Tex.Get(), 0, m_DepthSRV.GetAddressOf()));
}

void DepthBuffer::Clear()
{
	m_pContext->ClearDepthStencilView(m_DSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, m_ClearDepth, m_ClearStencil);
}
