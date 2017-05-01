#pragma once

#include "PixelBuffer.h"
#include "d3dUtil.h"

class DepthBuffer : public PixelBuffer
{
public:
	DepthBuffer(const std::string& Name);

	void Create(uint32_t Width, uint32_t Height, DXGI_FORMAT Format);
	void Resize(UINT Width, UINT Height);
	void Clear(bool bClearDepth = true, bool bClearStencil = true);
	ID3D11DepthStencilView* GetDSV() { return m_DSV.Get(); }

	std::string m_name;
	float m_ClearDepth = 1.0f;
	uint8_t m_ClearStencil = 0;
	bool m_Enable4xMsaa = false;
	uint8_t m_4xMsaaQuality = 0;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_Texture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_DSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_DepthStencilSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_StencilSRV;

private:
    ID3D11DeviceContext* m_pContext = nullptr;
};

using DepthBufferPtr = std::shared_ptr<DepthBuffer>;