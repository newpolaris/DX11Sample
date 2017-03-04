#include "ShaderFactoryDX11.h"

using Microsoft::WRL::ComPtr;

ComPtr<ID3DBlob> ShaderFactoryDX11::LoadShader(
	std::wstring & filename,
	std::wstring & function,
	std::wstring & model)
{
	std::ifstream fin(filename, std::ios::binary);

	fin.seekg(0, std::ios_base::end);
	auto size = static_cast<int>(fin.tellg());
	std::vector<char> compiledShader(size);
	fin.read(compiledShader.data(), size);
	fin.close();

	ComPtr<ID3DBlob> pBlob;
	HR(D3DCreateBlob(size, pBlob.GetAddressOf()));
	
	memcpy(pBlob->GetBufferPointer(), compiledShader.data(), size);

	return pBlob;
}

ComPtr<ID3DBlob> ShaderFactoryDX11::CompileShader(
	const std::wstring & filename,
	const D3D_SHADER_MACRO * defines,
	const std::string & function,
	const std::string & model)
{
    UINT flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> byteCode;
	ComPtr<ID3DBlob> errors;

	HRESULT hr = S_OK;
	HR(D3DX11CompileFromFile(filename.c_str(),
		defines,
		nullptr,
		function.c_str(),
		model.c_str(),
		flags,
		0,
		nullptr,
		byteCode.GetAddressOf(),
		errors.GetAddressOf(),
		&hr));

	return byteCode;
}
