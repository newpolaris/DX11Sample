#pragma once

#include "PixelBuffer.h"
#include "d3dUtil.h"
#include "Color.h"

class ColorBuffer;
using ColorBufferPtr = std::shared_ptr<ColorBuffer>;

class ColorBuffer : public PixelBuffer
{
public:
	ColorBuffer();

	void Create(std::string Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format, D3D11_SUBRESOURCE_DATA* Data = nullptr);
	void CreateFromSwapChain(std::string Name, Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture);
	void Clear();
	void Resize(UINT Width, UINT Height);
	void Destroy();
	void SetColor(Color ClearColor) { m_ClearColor = ClearColor; }
	ID3D11ShaderResourceView* GetSRV() { return m_SRV.Get(); }
	ID3D11RenderTargetView* GetRTV() { return m_RTV.Get(); }

	std::string m_name;
	Color m_ClearColor;
	uint32_t m_NumMipMaps; // number of texture sublevels
	uint32_t m_FragmentCount;
	uint32_t m_SampleCount;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_Tex;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_SRV;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RTV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_UAV;
};