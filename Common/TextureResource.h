#pragma once

#include "d3dUtil.h"

class TextureResource
{
public:
	TextureResource(ID3D11Device* device, std::string name);
	void LoadTextureFromFile(std::wstring filename);

	ID3D11ShaderResourceView* GetSRV() { return SRV.Get(); }

	ID3D11Device* pDevice;
	// Unique material name for lookup.
	std::string Name;
	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D11Resource> Resource;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
};

using TextureResourcePtr = std::shared_ptr<TextureResource>;