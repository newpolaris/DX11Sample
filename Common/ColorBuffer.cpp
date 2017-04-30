#include "ColorBuffer.h"
#include "d3dApp.h"

ColorBufferPtr ColorBuffer::CreateFromSwapChain(const std::string & Name, Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture)
{
	auto Buffer = std::make_shared<ColorBuffer>(Name);
	HR(g_Device->CreateRenderTargetView(Texture.Get(), nullptr, Buffer->m_RTV.ReleaseAndGetAddressOf()));

	return Buffer;
}

ColorBuffer::ColorBuffer(const std::string & Name)
	: m_name(Name), m_ClearColor(0.0f, 0.0f, 0.0f, 0.0f), m_NumMipMaps(0), m_FragmentCount(1), m_SampleCount(1) 
{
	g_Device->GetImmediateContext(&m_pContext);
}

void ColorBuffer::Create( uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format, D3D11_SUBRESOURCE_DATA* Data)
{
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

	Microsoft::WRL::ComPtr<ID3D11Texture2D> Tex = nullptr;
	HR(g_Device->CreateTexture2D(&TexDesc, Data, Tex.GetAddressOf()));
	Tex->SetPrivateData(WKPDID_D3DDebugObjectName, m_name.size(), m_name.c_str());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	HR(g_Device->CreateShaderResourceView(Tex.Get(), &srvDesc, m_SRV.ReleaseAndGetAddressOf()));

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Format = Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	HR(g_Device->CreateUnorderedAccessView(Tex.Get(), &uavDesc, m_UAV.ReleaseAndGetAddressOf()));

	HR(g_Device->CreateRenderTargetView(Tex.Get(), nullptr, m_RTV.ReleaseAndGetAddressOf()));
}

void ColorBuffer::Clear()
{
	FLOAT clearColor[4] = { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a };
	m_pContext->ClearRenderTargetView(m_RTV.Get(), clearColor);
}
