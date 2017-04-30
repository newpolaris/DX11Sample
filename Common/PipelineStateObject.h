#pragma once

#include "d3dUtil.h"
#include "RenderTarget.h"
#include "Shader.h"

struct BlendState
{
	BlendState() = default;
	BlendState(ID3D11BlendState* pState, std::array<FLOAT, 4> factor, UINT mask) 
		: pBlendState(pState), BlendFactor(factor), SampleMask(mask) {}
	ID3D11BlendState* pBlendState = nullptr;
	std::array<FLOAT, 4> BlendFactor = { 1.f, 1.f, 1.f, 1.f };
	UINT SampleMask = 0xFFFFFFFF;
};
struct DepthStencilState
{
	DepthStencilState() = default;
	DepthStencilState(ID3D11DepthStencilState* pState, UINT ref)
		: pDepthStencilState(pState), StencilRef(ref) {}
	ID3D11DepthStencilState* pDepthStencilState = nullptr;
	UINT StencilRef = 0;
};

struct PipelineBindings
{
	enum { MAX_TEXTURE_BINDINGS = 128, MAX_SAMPLER_BINDINGS = 16, MAX_BUFFER_BINDINGS = 128, MAX_CB_BINDINGS = 15 };
	std::array<std::string, MAX_SAMPLER_BINDINGS> Sampler;
};

struct PipelineStateDesc
{
	PipelineStateDesc(std::string il, std::string vs, std::string ps, std::string gs = "");
	void BindSampler(int ShaderFlags, const std::vector<std::string>& SamplerList);

	std::string IL;
	std::string m_ShaderName[NumShader];

	std::string BS;
	std::array<FLOAT, 4> BlendFactor = { 1.f, 1.f, 1.f, 1.f };
	UINT SampleMask = 0xFFFFFFFF;
	std::string DSS;
	UINT StencilRef = 0;
	std::string RS; // render state
	std::string RT; // render target

	PipelineBindings bindings[NumShader];
};

// Pasudo PSO
struct PipelineStateObject
{
	ID3D11InputLayout* pIL = nullptr;
	ShaderPtr m_Shaders[NumShader];
	BlendState blend;
	DepthStencilState depthStencil;
	ID3D11RasterizerState* pResterizer = nullptr;
	RenderTarget* pRenderTarget = nullptr;
	int nSamplerCount = 0;
	ID3D11SamplerState* pSamplerStates[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
};