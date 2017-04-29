#pragma once

#include "d3dUtil.h"
#include "ColorBuffer.h"
#include "DepthBuffer.h"

enum class Slot : uint8_t
{
	Color0,
	Color1,
	Color2,
	Color3,
	Color4,
	Color5,
	Color6,
	Color7,
	Count,	 // D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT
};

class RenderTarget
{
public:
	RenderTarget();
	void SetColor(Slot slot, ColorBufferPtr buffer);
	void SetDepth(DepthBufferPtr buffer);
	void Bind();
	void UnBind();

private:
	ID3D11DeviceContext* m_pContext = nullptr;
	DepthBufferPtr m_Depth;
	std::array<ColorBufferPtr, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> m_Color;	
};

using RenderTargetPtr = std::shared_ptr<RenderTarget>;