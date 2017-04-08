#pragma once

#include "d3dUtil.h"

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

struct PipelineStateDesc
{
	PipelineStateDesc(std::string il, std::string vs, std::string ps, std::string gs = "") 
	 : IL(il), VS(vs), PS(ps), GS(gs) {}
	std::string IL;
	std::string VS;
	std::string PS;
	std::string GS;
	std::string HS;
	std::string DS;

	std::string BS;
	std::array<FLOAT, 4> BlendFactor = { 1.f, 1.f, 1.f, 1.f };
	UINT SampleMask = 0xFFFFFFFF;
	std::string DSS;
	UINT StencilRef = 0;
	std::string RS;
};

// Pasudo PSO
struct PipelineStateObject
{
	ID3D11InputLayout* pIL = nullptr;
	ID3D11VertexShader* pVS = nullptr;
	ID3D11PixelShader* pPS = nullptr;
	ID3D11GeometryShader* pGS = nullptr;
	ID3D11DomainShader* pDS = nullptr;
	ID3D11HullShader* pHS = nullptr;
	BlendState blend;
	DepthStencilState depthStencil;
	ID3D11RasterizerState* pResterizer = nullptr;
};