#include "TextureResource.h"

TextureResource::TextureResource(ID3D11Device* device, std::string name)
	: pDevice(device), Name(name)
{
}

void TextureResource::LoadTextureFromFile(std::wstring filename)
{
	Filename = filename;

	ThrowIfFailed(D3DX11CreateShaderResourceViewFromFile(
		pDevice,
		filename.c_str(),
		0, 0,
		SRV.GetAddressOf(), 0));
}