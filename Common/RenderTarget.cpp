#include "RenderTarget.h"
#include "d3dApp.h"

RenderTarget::RenderTarget() 
{
	g_Device->GetImmediateContext(&m_pContext);
}

void RenderTarget::SetColor(Slot slot, ColorBufferPtr buffer)
{
	m_Color[static_cast<uint8_t>(slot)] = buffer;
}

void RenderTarget::SetDepth(DepthBufferPtr buffer)
{
	m_Depth = buffer;
}

void RenderTarget::Bind()
{
	for (uint8_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		if (m_Color[i] && m_bClearColor)
			m_Color[i]->Clear();

	if (m_Depth)
		m_Depth->Clear(m_bClearDepth, m_bClearStencil);

	std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs;
	uint8_t rtv_count = 0;
	for (uint8_t i = 0; i < rtvs.size(); i++)
		if (m_Color[i])
			rtvs[rtv_count++] = m_Color[i]->GetRTV();

	ID3D11DepthStencilView* dsv = nullptr;
	if (m_Depth)
		dsv = m_Depth->GetDSV();

	m_pContext->OMSetRenderTargets(rtv_count, rtvs.data(), dsv);
}

void RenderTarget::UnBind()
{
	m_pContext->OMSetRenderTargets(0, nullptr, nullptr);
}
