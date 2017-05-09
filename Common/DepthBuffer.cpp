#include "DepthBuffer.h"
#include "d3dApp.h"

DepthBuffer::DepthBuffer(const std::string & Name)
	: m_name(Name)
{
	g_Device->GetImmediateContext(&m_pContext);
}

DXGI_FORMAT GetDSVFormat( DXGI_FORMAT defaultFormat )
{
	switch (defaultFormat)
	{
	// 32-bit Z w/ Stencil
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	// No Stencil
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;

	// 24-bit Z
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 16-bit Z w/o Stencil
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
		return DXGI_FORMAT_D16_UNORM;

	default:
		return defaultFormat;
	}
}

DXGI_FORMAT GetDepthFormat( DXGI_FORMAT defaultFormat )
{
	switch (defaultFormat)
	{
	// 32-bit Z w/ Stencil
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

	// No Stencil
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;

	// 24-bit Z
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

	// 16-bit Z w/o Stencil
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
		return DXGI_FORMAT_R16_UNORM;

	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

DXGI_FORMAT GetBaseFormat( DXGI_FORMAT defaultFormat )
{
	switch (defaultFormat)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_TYPELESS;

	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_TYPELESS;

	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8X8_TYPELESS;

	// 32-bit Z w/ Stencil
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_R32G8X24_TYPELESS;

	// No Stencil
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;

	// 24-bit Z
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;

	// 16-bit Z w/o Stencil
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;

	default:
		return defaultFormat;
	}
}

void DepthBuffer::Create(uint32_t Width, uint32_t Height, DXGI_FORMAT Format)
{
	D3D11_TEXTURE2D_DESC DepthStencilDesc;
	DepthStencilDesc.Width     = Width;
	DepthStencilDesc.Height    = Height;
	DepthStencilDesc.MipLevels = 1;
	DepthStencilDesc.ArraySize = 1;
	DepthStencilDesc.Format    = GetBaseFormat(Format);

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
	DepthStencilDesc.BindFlags      = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	DepthStencilDesc.CPUAccessFlags = 0; 
	DepthStencilDesc.MiscFlags      = 0;

	HR(g_Device->CreateTexture2D(&DepthStencilDesc, 0, m_Texture.GetAddressOf()));
	m_Texture->SetPrivateData(WKPDID_D3DDebugObjectName, m_name.size(), m_name.c_str());

	D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc;
	depthViewDesc.Format = GetDSVFormat(Format);
	depthViewDesc.Flags = 0;
	depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthViewDesc.Texture2D.MipSlice = 0;
	HR(g_Device->CreateDepthStencilView(m_Texture.Get(), &depthViewDesc, m_DSV.GetAddressOf()));

	D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
	resourceViewDesc.Format = GetDepthFormat(Format);
	resourceViewDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
	resourceViewDesc.Texture2D.MipLevels = 1;
	resourceViewDesc.Texture2D.MostDetailedMip = 0;
	HR(g_Device->CreateShaderResourceView(m_Texture.Get(), &resourceViewDesc, m_DepthSRV.GetAddressOf()));
}

void DepthBuffer::Resize(UINT Width, UINT Height)
{
	assert(m_Texture);

	D3D11_TEXTURE2D_DESC Desc;
	m_Texture->GetDesc(&Desc);

	Create(Width, Height, Desc.Format);
}

void DepthBuffer::Clear(bool bClearDepth, bool bClearStencil)
{
	UINT ClearFlags = 0;
	if (bClearDepth)
		ClearFlags |= D3D11_CLEAR_DEPTH;
	if (bClearStencil)
		ClearFlags |= D3D11_CLEAR_STENCIL;
	if (ClearFlags)
		m_pContext->ClearDepthStencilView(m_DSV.Get(), ClearFlags, m_ClearDepth, m_ClearStencil);
}
