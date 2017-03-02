#pragma once

#include "d3dUtil.h"

class ShaderFactoryDX11
{
public:
	ShaderFactoryDX11() = delete;
	~ShaderFactoryDX11() = delete;
	static Microsoft::WRL::ComPtr<ID3DBlob> LoadShader(
		std::wstring& filename,
		std::wstring& function,
		std::wstring& model);
	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target);
};