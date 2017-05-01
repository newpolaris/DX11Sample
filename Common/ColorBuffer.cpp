#include "ColorBuffer.h"
#include "d3dApp.h"

using Microsoft::WRL::ComPtr;

ColorBuffer::ColorBuffer()
	: m_ClearColor(0.0f, 0.0f, 0.0f, 0.0f), m_NumMipMaps(0), m_FragmentCount(1), m_SampleCount(1) 
{
}

void ColorBuffer::Create(std::string Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format, D3D11_SUBRESOURCE_DATA * Data)
{
	m_name = Name;

	D3D11_TEXTURE2D_DESC TexDesc;
	TexDesc.Width     = Width;
	TexDesc.Height    = Height;
    TexDesc.MipLevels = 1;
    TexDesc.ArraySize = 1;
	TexDesc.Format    = Format;
	TexDesc.SampleDesc.Count   = 1;
	TexDesc.SampleDesc.Quality = 0;
    TexDesc.Usage     = D3D11_USAGE_DEFAULT;
	// TODO: Format에 맞춰서 되는 것만 enable 하게 바꿔야 함
    TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;
    TexDesc.CPUAccessFlags = 0;
    TexDesc.MiscFlags      = 0;

	HR(g_Device->CreateTexture2D(&TexDesc, Data, m_Tex.GetAddressOf()));
	m_Tex->SetPrivateData(WKPDID_D3DDebugObjectName, m_name.size(), m_name.c_str());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	HR(g_Device->CreateShaderResourceView(m_Tex.Get(), &srvDesc, m_SRV.ReleaseAndGetAddressOf()));

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Format = Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	HR(g_Device->CreateUnorderedAccessView(m_Tex.Get(), &uavDesc, m_UAV.ReleaseAndGetAddressOf()));
	HR(g_Device->CreateRenderTargetView(m_Tex.Get(), nullptr, m_RTV.ReleaseAndGetAddressOf()));
}

void ColorBuffer::CreateFromSwapChain(std::string Name, Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture)
{
	m_name = Name;
	HR(g_Device->CreateRenderTargetView(Texture.Get(), nullptr, m_RTV.ReleaseAndGetAddressOf()));
}

void ColorBuffer::Resize(UINT Width, UINT Height)
{
	assert(m_Tex);

	D3D11_TEXTURE2D_DESC Desc;
	m_Tex->GetDesc(&Desc);

	Create(m_name, Width, Height, Desc.MipLevels, Desc.Format);
}

void ColorBuffer::Clear()
{
	FLOAT clearColor[4] = { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a };

	ComPtr<ID3D11DeviceContext> pContext;
	g_Device->GetImmediateContext(&pContext);
	assert(m_RTV.Get());
	pContext->ClearRenderTargetView(m_RTV.Get(), clearColor);
}

void ColorBuffer::Destroy()
{
	m_SRV = nullptr;
	m_RTV = nullptr;
	m_UAV = nullptr;
}
